#include <stddef.h> /* size_t */
#include <stdint.h>
#include <string.h> /* memcpy */

#include "scsi_sd.h"
#include "sd.h"
#include "scsi/scsi.h"
#include "chs.h"
#include "endian.h"

#include "serialize.h" /* logging */


/******************************************************************************/


#define UNUSED(var) ((void) (var))

#define IO_BUFFER_SIZE (SD_BLOCK_SIZE * 8)
#if IO_BUFFER_SIZE == 0 || IO_BUFFER_SIZE % SD_BLOCK_SIZE != 0
#error buffer byte count must be a multiple of the block size and > than 0
#endif


/******************************************************************************/


typedef struct { 
    uint32_t   lba;       /* starting lba for this lun */
    uint32_t   count;     /* number of blocks in the lun */
} lun_t;


/******************************************************************************/


/*--- SD CARD INFORMATION ----------------------------------------------------*/
static int      _initialized    = 0;

/*--- STATE ------------------------------------------------------------------*/
/* information on what luns are set and which one is selected */
static lun_t    _luns[1]        = { {0, 0} };
static lun_t   *_lun            = NULL;

/*--- CDB STATE --------------------------------------------------------------*/
/* what command descriptor block are we currently working on? */
static const scsi_cdb_t *_cdb = NULL;

/* for read and write operations, keep track of how many blocks we have 
   read/written from the lba specified in `_cdb` */
static size_t _lba_offset; 

/*--- DATA IN/OUT OPERATIONS -------------------------------------------------*/
static struct {
    size_t count;                       /* # of valid bytes in the buffer     */
                                        /**** state info for `scsi_sd_out` ****/
    uint8_t *read_ptr;                  /* next spot to read bytes from       */
    size_t read_count;                  /* # of valid bytes from `read_ptr`   */
                                        /**** state info for `scsi_sd_in`  ****/
    uint8_t *write_ptr;                 /* where to put the next IN data      */
    size_t write_count;                 /* how much free space is left        */
    
    uint8_t bytes[IO_BUFFER_SIZE];
} _io = {0};

/*--- REPORT LUNS DATA -------------------------------------------------------*/
static const report_luns_parameter_data_t _report_luns_data = {
    .lun_list_length = htobe32(1),
    ._reserved       = 0,
    .lun_list        = { 0 }
};

/*--- INQUIRY DATA -----------------------------------------------------------*/
/* scsi spc 4.4.1 - all left aligned ascii strings will have extra bytes filled
 * with 0x20 aka ' ' a space. */
static const inquiry_data_t _inquiry_data = {
    .peripheral           = INQUIRY_DATA_PDT_DABD | INQUIRY_DATA_PQ_CONNECTED,
    .removable            = INQUIRY_DATA_RMB_REMOVABLE,
    .version              = INQUIRY_DATA_VERSION_NON_STANDARD,
    .response_data_format = 0x01, // UNKNOWN obsolete value
    .additional_length    = INQUIRY_DATA_ADDITIONAL_LENGTH(sizeof(_inquiry_data)),
    .flags                = {0, 0, 0},
    .vendor_id            = {'T','e','e','n','s','y',' ',' '},
    .product_id           = {'U','S','B',' ','M','I','C','R','O',' ','S','D',' ',' ',' ',' '},
    .product_revision     = {'M','S','D','1'}
};
/*--- SENSE DATA: FIXED FORMAT -----------------------------------------------*/
static fixed_format_sense_data_t _ffsd = FIXED_FORMAT_SENSE_DATA_DEFAULT;

/*--- MODE PAGE: FLEXIBLE DISK -----------------------------------------------*/
static mode_page_flexible_disk_t _fdmp = { 0 };


/******************************************************************************/


/*--- DATA VALIDATION --------------------------------------------------------*/
/* generic test, everything is of valid sizes and sane values */
static int is_valid_cdb(const scsi_cdb_t *cdb, size_t length);
/* checks if the command can be completed at this time, ex is there an sd card
   to read from. Returns 1 if the command can be completed, 0 otherwise */
static int in_state_to_complete(const scsi_cdb_t *cdb);

/*--- SCSI OPERATIONS --------------------------------------------------------*/
/* code for parsing and completing the differnt SCSI Command CDBs supported   */
static ssize_t format_unit(const void *cdb);
static ssize_t inquiry(const void *cdb);
static ssize_t load_unload(const void *cdb);
static ssize_t mode_sense6(const void *cdb);
static ssize_t prevent_allow_medium_removal(const void *cdb);
static ssize_t read6(const void *cdb);
static ssize_t read10(const void *cdb);
static ssize_t read_capacity10(const void *cdb);
static ssize_t read_format_capacities(const void *cdb);
static ssize_t report_luns(const void *cdb);
static ssize_t request_sense(const void *cdb);
static ssize_t send_diagnostic(const void *cdb);
static ssize_t test_unit_ready(const void *cdb);
static ssize_t write6(const void *cdb);
static ssize_t write10(const void *cdb);

/*--- READ/WRITE OPERATIONS --------------------------------------------------*/
/* handles reading and writing data from sd card to/from our buffer */
static int scsi_read(uint32_t lba, size_t bcount);
static int scsi_write(uint32_t lba, size_t bcount);

/*--- SCSI SENSE OPERATIONS --------------------------------------------------*/
/* update the request sense data to tell the host what type of error happened
   asc_ascq will be put into the correct byte order */
static void set_sense(uint8_t sense_key, uint16_t asc_ascq);

/*--- BUFFERED IO OPERATIONS -------------------------------------------------*/
static void   io_reset(void);
static size_t io_read(void **dest, size_t maxlen);
static int    io_write(const void *src, size_t length);
/* since the allocation_length in scsi cdbs can limit the number of bytes to be
   sent, `io_limit` will alter the # of bytes avaialable for reading if the
   `allocation_length` is < bytes in the buffer. see `mode_sense6`. Returns the
   number of readable bytes after it is limited */
static size_t io_limit(size_t allocation_length);


/******************************************************************************/


/*--- INIT -------------------------------------------------------------------*/
int scsi_sd_init(void) 
{
    size_t max_lba;
    chslimits_t limits;
    
    if (sd_init() != 0) { return -1; }
    
    max_lba = sd_max_lba();
    LOGINFO("Max LBA 0x%08x", max_lba);
    LOGINFO("Block Size %u (0x%04x) bytes", SD_BLOCK_SIZE, SD_BLOCK_SIZE);
    LOGINFO("Size %u (0x%08x) bytes", 
        max_lba * SD_BLOCK_SIZE, max_lba * SD_BLOCK_SIZE);
    
    /* split the sd card into 2 */
    _luns[0] = (lun_t) { 0, max_lba };
    _lun = &_luns[0];
    
    LOGINFO("LUN 0  0x%x (%u blocks)", _luns[0].lba, _luns[0].count);
    
    
    LOGINFO("initializing the flexible data mode page");
    /* calculate CHS limits for an lba = our max lba */
    lba2chslimits(&limits, _lun->count);
    _fdmp.page_code          = FLEXIBLE_DISK_PAGE_CODE;
    _fdmp.page_length        = FLEXIBLE_DISK_PAGE_LENGTH;
    _fdmp.transfer_rate      = htobe16(0x3c00); // TODO value from an sd reader
    _fdmp.head_count         = limits.head_count;
    _fdmp.track_sector_count = limits.track_sector_count;
    _fdmp.sector_byte_count  = htobe16(SD_BLOCK_SIZE);
    _fdmp.cylinder_count     = htobe16(limits.cylinder_count);
    
    /**/
    _ffsd = FIXED_FORMAT_SENSE_DATA_DEFAULT;
    
    /* flag to tell rest of the impl we are ready */
    _initialized = 1;
    
    /* on initization tell the host the medium has changed */
    set_sense(
        SENSE_KEY_UNIT_ATTENTION, ASC_ASCQ_NOT_READY_MEDIUM_MAY_HAVE_CHANGED
    );
    
    return 0;
}


/*--- START TRANSACTION ------------------------------------------------------*/
ssize_t scsi_sd_begin(const void *cdb, size_t cdblen) 
{
    if (!is_valid_cdb(cdb, cdblen)) 
    {
        LOGERROR("invalid cdb recieved");
        sxxd(cdb, cdblen);
        return -1;
    }
    
    /* initialize all state information */
    _cdb        = cdb;
    _lba_offset = 0;
    io_reset();
    
    if (!in_state_to_complete(cdb)) { return -1; }
    
    switch (_cdb->opcode) 
    {
    case FORMAT_UNIT_OPCODE:                  return format_unit(cdb);
    case INQUIRY_OPCODE:                      return inquiry(cdb);
    case LOAD_UNLOAD_OPCODE:                  return load_unload(cdb);
    case MODE_SENSE6_OPCODE:                  return mode_sense6(cdb);
    case PREVENT_ALLOW_MEDIUM_REMOVAL_OPCODE: return prevent_allow_medium_removal(cdb);
    case READ6_OPCODE:                        return read6(cdb);
    case READ10_OPCODE:                       return read10(cdb);
    case READ_CAPACITY10_OPCODE:              return read_capacity10(cdb);
    case READ_FORMAT_CAPACITIES_OPCODE:       return read_format_capacities(cdb);
    case REPORT_LUNS_OPCODE:                  return report_luns(cdb);
    case REQUEST_SENSE_OPCODE:                return request_sense(cdb);
    case SEND_DIAGNOSTIC_OPCODE:              return send_diagnostic(cdb);
    case TEST_UNIT_READY_OPCODE:              return test_unit_ready(cdb);
    case WRITE6_OPCODE:                       return write6(cdb); 
    case WRITE10_OPCODE:                      return write10(cdb);
    
    default:
        /* if we get here then the command is not supported */
        LOGERROR("unsupported SCSI opcode 0x%02x",_cdb->opcode);
        sxxd(cdb, cdblen);
        set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_INVALID_COMMAND);
        return -1;
    }
}

/* return 1 if cdb is valid, 0 otherwise */
int is_valid_cdb(const scsi_cdb_t *cdb, size_t cdblen) 
{
    if (cdb == NULL) 
    {
        LOGERROR("cdb is NULL");
        set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
        return 0;
    }
    
    /* windows sets cdblen to 12 for these 6 byte cdbs, just accept */
    if ((cdb->opcode == REQUEST_SENSE_OPCODE || cdb->opcode == INQUIRY_OPCODE)
            && cdblen == 12) 
    {
        return 1;
    }
    
    /* validate cdb lengths */
    switch (cdb->opcode & GROUP_CODE_MASK) 
    {
    case GROUP_CODE_CDB6:
        if (cdblen != 6) {
            set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_INVALID_COMMAND);
            return 0;
        }
        break;
        
    /* there are 2 group codes for 10 byte CDBs */
    case GROUP_CODE_CDB10:
    case GROUP_CODE_CDB10_2:
        if (cdblen != 10) {
            set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_INVALID_COMMAND);
            return 0;
        }
        break;
        
    case GROUP_CODE_CDB12:
        if (cdblen != 12) {
            set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_INVALID_COMMAND);
            return 0;
        }
        break;
        
    /* we currently don't support any 16|32|variable length CDBs */
    default:
        set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_INVALID_COMMAND);
        return 0;
    }
    
    /* cdb is valid if we get here */
    return 1;
}

int in_state_to_complete(const scsi_cdb_t *cdb) 
{
    /* the following commands can be completed even if communication with sd 
       card is not working */
    switch (cdb->opcode) 
    {
        case INQUIRY_OPCODE:
        case REPORT_LUNS_OPCODE:
        case REQUEST_SENSE_OPCODE:
        case SEND_DIAGNOSTIC_OPCODE:
        case TEST_UNIT_READY_OPCODE:
            return 1;
            
        default: 
            return _initialized;
    }
}

/*--- SCSI SD DATA OUT -------------------------------------------------------*/
ssize_t scsi_sd_data_out(void **ptr, size_t maxlen) 
{
    ssize_t ret;
    size_t count;
    
    /* if we didn't initialize successfully, and the opcode is not one of the 
       ones supported in an uninitialized state, error */
    if (!in_state_to_complete(_cdb)) 
    {
        set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_LUN_NOT_READY);
        return -1;
    }
    
    /* if we are dealing with a DATA IN cdb, ERROR */
    if (_cdb->opcode == WRITE6_OPCODE || _cdb->opcode == WRITE10_OPCODE) 
    {
        LOGCRITICAL("`scsi_sd_data_out` called with IN cdb");
        set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
        return -1;
    }
    
    /* the buffer is always filled if there are >= IO_BUFFER_SIZE bytes to 
       be processed in the data OUT phase. Therefore if the # of valid bytes in
       the buffer is less than max data OUT is done once we empty the buffer */
    if (_io.count < IO_BUFFER_SIZE) 
    {
        count = io_read(ptr, maxlen);
        /* no data to read, and the buffer wasn't filled, signal no more bytes*/
        if (count == 0) { *ptr = NULL; }
        return count;
        
    } else {
        /* The buffer is full! We must check if there are more bytes to be sent
           once we hand off all the bytes in the buffer to the caller */
        if (_io.read_count == 0) 
        {
            io_reset(); /* reset the buffer counters to be filled */
            switch (_cdb->opcode) 
            {
                case READ6_OPCODE:  ret = read6(_cdb);  break;
                case READ10_OPCODE: ret = read10(_cdb); break;
                default:
                    LOGCRITICAL("buffer filled by non read opcode: 0x%02hhx",
                        _cdb->opcode);
                    set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
                    return -1;
            }
            
            /* make sure refilling the buffer was successfull */
            if (ret < 0) 
            {
                LOGERROR("failed to refill the buffer");
                return ret;
            }
        }
        count = io_read(ptr, maxlen);
        
        /* if there are no more bytes to send set `ptr` to NULL */
        if (count == 0) { *ptr = NULL; }
        return count;
    }
}

/**** SCSI SD DATA IN *********************************************************/
int scsi_sd_data_in(const void *src, size_t length) 
{
    
    /* validate we are initialized and the cbw opcode is valid */
    if (!_initialized) 
    {
        set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_LUN_NOT_READY);
        return -1;
    }
    
    if (_cdb->opcode != WRITE6_OPCODE && _cdb->opcode != WRITE10_OPCODE) 
    {
        LOGCRITICAL("`scsi_sd_data_in` called with an OUT opcode: 0x%02hhx", 
            _cdb->opcode);
        set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
        return -1;
    }
    
    /* we are initialized and it's a write opcode */
    
    if (io_write(src, length) != 0) 
    {
        LOGCRITICAL("buffer is full??????????");
        set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
        return -1;
    }
    
    /* if the buffer is now full flush it */
    if (_io.count == IO_BUFFER_SIZE) 
    {
        if (scsi_sd_data_in_commit() < 0) 
        {
            LOGERROR("failed to commit full buffer to sd card");
            return -1;
        }
    }
    return 0;
}

ssize_t scsi_sd_data_in_commit(void) 
{
#define ERROR_BYTES_WRITTEN(c) (-1 * ((c) + 1))
    ssize_t ret;
    
     /* validate we are initialized and the cbw opcode is valid */
    if (!_initialized) 
    {
        set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_LUN_NOT_READY);
        return ERROR_BYTES_WRITTEN(_lba_offset * SD_BLOCK_SIZE);
    }
    
    switch (_cdb->opcode) 
    {
        case WRITE6_OPCODE:  ret = write6(_cdb);  break;
        case WRITE10_OPCODE: ret = write10(_cdb); break;
        default:
            LOGCRITICAL("`scsi_sd_data_in_commit` called with OUT cdb");
            set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
            return ERROR_BYTES_WRITTEN(_lba_offset * SD_BLOCK_SIZE);
    }
    
    if (ret < 0) 
    {
        LOGERROR("write6/10 failed");
        return ERROR_BYTES_WRITTEN(_lba_offset * SD_BLOCK_SIZE);
    }
    
    /* all possible blocks were written, clear the buffer to allow more data 
       to be sent in */
    io_reset();
    return _lba_offset * SD_BLOCK_SIZE;
#undef ERROR_BYTES_WRITTEN
}

/******************************************************************************/

ssize_t format_unit(const void *cdbptr) 
{
    UNUSED(cdbptr);
    LOGINFO("SCSI FORMAT UNIT");
    /* this command is required but is meaningless to us, therefore report a
       failure to format the medium if the host tries to use this command */
    /* TODO research the appropriate way to handle this request */
    set_sense(SENSE_KEY_MEDIUM_ERROR, ASC_ASCQ_FORMAT_COMMAND_FAILED);
    return -1;
}

ssize_t inquiry(const void *cdbptr) 
{
    UNUSED(cdbptr);
    LOGINFO("SCSI INQUIRY");
    /* io_write should not fail since this is called after a io_reset */
    io_write(&_inquiry_data, sizeof(_inquiry_data));
    return sizeof(_inquiry_data);
}

ssize_t load_unload(const void *cdbptr) 
{
    UNUSED(cdbptr);
    LOGINFO("SCSI LOAD UNLOAD");
    /* do nothing and return success */
    return 0;
}

ssize_t mode_sense6(const void *cdbptr) 
{
    const mode_sense6_t *cdb;
    mode_parameter_header6_t mph6;
    size_t allocation_length;
    size_t length;
    
    LOGINFO("SCSI MODE SENSE (6)");
   
    cdb = cdbptr;
    allocation_length = cdb->allocation_length;
    
    /* return success scsi spc 3r23 p29 4.3.4.6 */
    if (allocation_length == 0) 
    {
        return 0;
    }
    
    /* no matter what this is the base header that will be sent */
    mph6 = (mode_parameter_header6_t) {
        .mode_data_length           = 3,
        .medium_type                = MODE_PARAMETER_MEDIUM_TYPE_DABD,
        .device_specific_parameter  = 0,
        .block_descriptor_length    = 0
    };
    
    /* build the response, as observed by the kingston dt 101 g2 upon recieving
       an unsupported page_code just return a empty header */
    switch (cdb->pc_page_code & MODE_SENSE6_PAGE_CODE_MASK) 
    {
    case MODE_SENSE_PAGE_CODE_RETURN_ALL:
        /* send the header and mode page */
        length = sizeof(mph6) + sizeof(_fdmp);
        
        /* scsi spc 3r23 p29 4.3.4.6 */
        if (length > CDB6_ALLOCATION_LENGTH_MAX) 
        {
            set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_INVALID_FIELD_IN_CDB);
            return -1;
        }
        
        mph6.mode_data_length = length - sizeof(mph6.mode_data_length);
        
        io_write(&mph6,  sizeof(mph6));
        io_write(&_fdmp, sizeof(_fdmp));
        return io_limit(allocation_length);
        
    default:
        LOGERROR("unsupported pagecode/subpage requested 0x%02x/0x02x", 
            cdb->pc_page_code & MODE_SENSE6_PAGE_CODE_MASK, cdb->subpage_code);
        io_write(&mph6, sizeof(mph6));
        return io_limit(allocation_length);
    }
}

/* not sure appropriate way to handle, following what other flash drives do */
ssize_t prevent_allow_medium_removal(const void *cdbptr) 
{
    const prevent_allow_medium_removal_t *cdb = cdbptr;
    LOGINFO("SCSI PREVENT ALLOW MEDIUM REMOVAL ");
    if (cdb->prevent & PREVENT_ALLOW_MEDIUM_REMOVAL_PREVENT_DTE) 
    {
        set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_INVALID_FIELD_IN_CDB);
        return -1;
    }
    return 0;
}

ssize_t read6(const void *cdbptr) 
{
    const read6_t *cdb;
    uint32_t lba;
    uint16_t count;
    
    cdb = cdbptr;
    lba = (be32toh(cdb->lba_transfer_length)&READ6_LBA_MASK)>>READ6_LBA_SHIFT;
    if ((count = cdb->transfer_length) == 0) 
    {
        count = 256; /* scsi-sbc-344r0 p45 5.5 */
    }
    
    /* since we are using an ssize_t to report bytes written instead of a size_t
       we can only report on 2 billion bytes */
    if (((uint32_t) count) * SD_BLOCK_SIZE > INT32_MAX - 1) 
    {
        LOGCRITICAL("integer overflow, transfer > 2GiB");
        set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
        return -1;
    }
    
    if (scsi_read(lba, count) < 0) { return -1; }
    return count * SD_BLOCK_SIZE;
}

ssize_t read10(const void *cdbptr) 
{
    const read10_t *cdb = cdbptr;
    uint32_t lba;
    uint16_t count;
    
    cdb = cdbptr;
    lba = be32toh(cdb->lba);
    count = be16toh(cdb->transfer_length);
    
    /* since we are using an ssize_t to report bytes written instead of a size_t
       we can only report on 2 billion bytes */
    if (((uint32_t) count) * SD_BLOCK_SIZE > INT32_MAX - 1) 
    {
        LOGCRITICAL("integer overflow, transfer > 2GiB");
        set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
        return -1;
    }
    
    if (scsi_read(lba, count) < 0) { return -1; }
    return count * SD_BLOCK_SIZE;
}

ssize_t read_capacity10(const void *cdbptr) 
{
    const read_capacity10_t *cdb;
    read_capacity10_data_t data;
    
    LOGINFO("SCSI READ CAPACITY (10)");
    
    cdb = cdbptr;
    
    if (cdb->pmi & READ_CAPACITY10_PMI_MASK) 
    {
        LOGERROR("PMI set, cannot handle");
        set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_INVALID_FIELD_IN_CDB);
        return -1;
    }
    
    data = (read_capacity10_data_t) {
        .lba          = htobe32(_lun->count - 1), /* last readable lba */
        .block_length = htobe32(SD_BLOCK_SIZE)
    };
    
    io_write(&data, sizeof(data));
    return sizeof(data);
}

ssize_t read_format_capacities(const void *cdbptr) 
{
    const read_format_capacities_t *cdb;
    capacity_list_header_t header;
    capacity_descriptor_t cd;
    uint16_t allocation_length;
    
    LOGINFO("SCSI READ FORMAT CAPACITIES");
    
    cdb = cdbptr;
    allocation_length = be16toh(cdb->allocation_length);
    
    header = (capacity_list_header_t) { { 0 }, CapacityListLength(1) };
    cd = (capacity_descriptor_t) {
        htobe32(_lun->count),
        htobe32(CapacityDescriptorTypeAndLength(DESCRIPTOR_TYPE_FORMATTED, SD_BLOCK_SIZE))
    };
    
    io_write(&header, sizeof(header));
    io_write(&cd, sizeof(cd));
    return io_limit(allocation_length);
}

ssize_t report_luns(const void *cdbptr) 
{
    const report_luns_t *cdb;
    uint32_t length;
    
    LOGINFO("SCSI REPORT LUNS");
    
    cdb = cdbptr;
    length = be32toh(cdb->allocation_length);
    if (length > sizeof(_report_luns_data)) 
    {
        length = sizeof(_report_luns_data);
    }
    io_write(&_report_luns_data, length);
    return length;
}

ssize_t request_sense(const void *cdbptr) 
{
    const request_sense_t *cdb;
    
    LOGINFO("SCSI REQUEST SENSE");
    
    cdb = cdbptr;
    
    if (cdb->desc & REQUEST_SENSE_DESC_MASK) 
    {
        LOGERROR("descriptor sense data requested, cannot handle");
        set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_INVALID_FIELD_IN_CDB);
        return -1;
    }
    
    io_write(&_ffsd, FIXED_FORMAT_SENSE_DATA_LENGTH);
    
    /* clear the sense data */
    _ffsd = FIXED_FORMAT_SENSE_DATA_DEFAULT;
    
    return FIXED_FORMAT_SENSE_DATA_LENGTH;
}

ssize_t send_diagnostic(const void *cdbptr) 
{
    UNUSED(cdbptr);
    LOGINFO("SCSI SEND DIAGNOSTIC");
    return 0;   /* just report success */
}

ssize_t test_unit_ready(const void *cdbptr) 
{
    UNUSED(cdbptr);
    LOGINFO("SCSI TEST UNIT READY");
    
    if (!_initialized) 
    {
        set_sense(SENSE_KEY_NOT_READY, ASC_ASCQ_MEDIUM_NOT_PRESENT);
        return -1;
    } 
    
    /* if there is a pending sense data we are not ready */
    if (_ffsd.asc != SENSE_KEY_NO_SENSE) { return -1; }
    
    return 0;
}

ssize_t write6(const void *cdbptr) 
{
    const write6_t *cdb;
    uint32_t lba;
    uint16_t count;
    
    cdb = cdbptr;
    lba = (be32toh(cdb->lba_transfer_length) & WRITE6_LBA_MASK) >> WRITE6_LBA_SHIFT;
    if ((count = cdb->transfer_length) == 0) 
    {
        count = 256; /* scsi-sbc-344r0 p77 5.24 */
    }
    
    /* since we are using an ssize_t to report bytes written instead of a size_t
       we can only report on 2 billion bytes */
    if (((uint32_t) count) * SD_BLOCK_SIZE > (INT32_MAX - 1)) 
    {
        LOGCRITICAL("integer overflow, transfer > 2GiB");
        set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
        return -1;
    }
    
    if (scsi_write(lba, count) < 0) { return -1; }
    return count * SD_BLOCK_SIZE;
}

int write10(const void *cdbptr) 
{
    const write10_t *cdb = cdbptr;
    uint32_t lba;
    uint16_t count;
    
    cdb   = cdbptr;
    lba   = be32toh(cdb->lba);
    count = be16toh(cdb->transfer_length);
    
    /* since we are using an ssize_t to report bytes written instead of a size_t
       we can only report on 2 billion bytes */
    if (((uint32_t) count) * SD_BLOCK_SIZE > (INT32_MAX - 1)) 
    {
        LOGCRITICAL("integer overflow, transfer > 2GiB");
        set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
        return -1;
    }
    
    if (scsi_write(lba, count) < 0) { return -1; }
    return count * SD_BLOCK_SIZE;
}

/******************************************************************************/

void set_sense(uint8_t sense_key, uint16_t asc_ascq) 
{
    _ffsd.response_code= FixedFormatResponseCode(0,RESPONSE_CODE_CURRENT_FIXED);
    _ffsd.sense_key = FixedFormatSenseKey(0, 0, 0, sense_key);
    _ffsd.asc_ascq = htobe16(asc_ascq);
}

int scsi_read(uint32_t lba, size_t block_count) 
{
    /* only report the read on the first invocation of scsi_read */
    if (_lba_offset == 0) 
    {
        LOGINFO("SCSI READ  %4hu blocks starting at lba 0x%08x",
            (uint16_t) block_count, lba); 
    }
    
    if ((lba + block_count) > _lun->count) 
    {
        set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_LBA_OUT_OF_RANGE);
        return -1;
    }
 
    /* the last call to scsi_read finished reading all the blocks requested */
    if (_lba_offset == block_count) 
    {
        return 0;
    }
 
    if (_lba_offset > block_count) 
    {
        LOGCRITICAL("_lba_offset > block_count: how did we get here?");
        set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
        return -1;
    }
    
    /* while the cdb wants us to read more blocks and we have enough space in
       the buffer */
    while (_lba_offset < block_count && _io.write_count >= SD_BLOCK_SIZE) 
    {
        if (sd_read_block(_io.write_ptr, lba + _lba_offset)) 
        {
            LOGERROR("reading lba 0x%08x", lba + _lba_offset);
            set_sense(SENSE_KEY_MEDIUM_ERROR,ASC_ASCQ_UNRECOVERD_READ_ERROR);
            return -1;
        }

        /* update counts and write pointer */
        _io.count       += SD_BLOCK_SIZE;
        _io.read_count  += SD_BLOCK_SIZE;
        _io.write_ptr   += SD_BLOCK_SIZE;
        _io.write_count -= SD_BLOCK_SIZE;
        _lba_offset++;
    }
    return 0;
}

int scsi_write(uint32_t lba, size_t block_count) 
{
    void *next;
    size_t count;
    
    /* if this is the first call to scsi_write for this cdb */
    if (_lba_offset == 0 && _io.count == 0) 
    { 
        LOGINFO("SCSI WRITE %hu blocks starting at lba 0x%08x", 
            (uint16_t) block_count, lba); 
    }
    
    /* scsi spec requires LBA OUT OF RANGE if the math doesn't make sense */
    if ((lba + block_count) > _lun->count) 
    {
        set_sense(SENSE_KEY_ILLEGAL_REQUEST, ASC_ASCQ_LBA_OUT_OF_RANGE);
        return -1;
    }
    
    /* we have already written all the expected blocks return success */
    if (_lba_offset == block_count) 
    {
        return 0;
    }
 
    /* possible situation, serious error would need to happen to get here */
    if (_lba_offset > block_count) 
    {
        LOGCRITICAL("_lba_offset > block_count: how did we get here?");
        set_sense(SENSE_KEY_HARDWARE_ERROR, 0x0000);
        return -1;
    }
    
    while (_lba_offset < block_count) 
    {
        count = io_read(&next, SD_BLOCK_SIZE);
        
        /* there is nothing in the buffer so no work to be done */
        if (count == 0) { break; }
        
        /* buffer contains a partially filled block, assume we are to recieve 
           more data and we will be called again */
        if (count != SD_BLOCK_SIZE) 
        {
            LOGWARN("`io_read` non block size (512B) bytes: %u", count);
            /* undo the read */
            _io.read_ptr   -= count;
            _io.read_count -= count;
            break;
        }
        
        if (sd_write_block(lba + _lba_offset, next)) 
        {
            LOGERROR("failed to write lba 0x%08x", lba + _lba_offset);
            set_sense(SENSE_KEY_MEDIUM_ERROR, ASC_ASCQ_PERIPHERAL_DEVICE_WRITE_FAULT);
            return -1;
        }
        
        _lba_offset++;
    }
    
    return 0;
}


/******************************************************************************/


void io_reset(void) 
{
    _io.count       = 0;
    _io.read_ptr    = (void *) _io.bytes;
    _io.read_count  = 0;
    _io.write_ptr   = (void *) _io.bytes;
    _io.write_count = IO_BUFFER_SIZE;
}

int io_write(const void *src, size_t length) 
{
    if (length > _io.write_count) { return -1; }
    memcpy(_io.write_ptr, src, length);
    _io.count       += length;
    _io.read_count  += length;
    _io.write_ptr   += length;
    _io.write_count -= length;
    return 0;
}

size_t io_read(void **dest, size_t maxlen) 
{
    if (_io.read_count < maxlen) { maxlen = _io.read_count; }
    *dest = _io.read_ptr;
    _io.read_ptr   += maxlen;
    _io.read_count -= maxlen;
    return maxlen;
}

size_t io_limit(size_t allocation_length) 
{
    if (allocation_length < _io.read_count) 
    {
        _io.read_count = allocation_length;
    }
    return _io.read_count;
}
