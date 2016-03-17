/* 
 * usb_msd is a bridge between usb communication and a microsd card used to 
 * store data. usb_msd handles the Command Block Wrappers (CBWs) and Command 
 * Status Wrappers (CSWs) and the handling of IO over the endpoints.
 */
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "usb_bdt.h"

#include "serialize.h"
#include "usb_dev.h"
#include "usb_msd.h"
#include "endian.h"
#include "scsi_sd.h"

/******************************************************************************/

/* buffers for the RX bdt entries of endpoint 1 */
static uint8_t _ep1_rx[2][EP1_SIZE] __attribute__((aligned(4)));

/* 
 * switched to int after difficulty with the compiler with these as uint8_t
 * the toggles were not being updated and causing communication on the TX endpt
 * to stop. Behavior was DATA0/EVEN, DATA1/ODD, DATA0/EVEN, DATA0/EVEN
 */
static int _ep2_data_toggle = DATA0;
static int _ep2_odd_toggle  = EVEN;

/* what msd phase are we in */
static enum { NONE, COMMAND_PHASE, DATA_PHASE, STATUS_PHASE } _phase = NONE;
static struct usb_msd_cbw _cbw = {0};
static struct usb_msd_csw _csw = {0};

/* # of bytes the host has sent or recieved in the data phase */
static size_t _bytes_recieved  = 0;
static size_t _bytes_sent      = 0;

/* # of bytes the SCSI CDB is expecting */
static size_t _bytes_device    = 0;


/******************************************************************************/

static int 
is_valid_cbw(const struct usb_msd_cbw *cbw, uint16_t length, uint8_t max_lun);

static void begin_transaction(const void *data, uint16_t length);
/* callbacks for the IN/TX endpoint to inform when data has been sent */
static void msd_tx_success(size_t bytes_sent);
static void msd_rx_success(void *bytes, size_t count);

static void send_status(uint8_t status, size_t processed);

/* using `scsi_buffer` setup the next block for transmission */
static void transmit_next(void); 
/* queues data for transmission in the bdt and updates toggles */
static void ep2_transmit(const void *data, size_t length);

static inline const char *phase2string(int phase) 
{
    switch (phase) 
    {
    case NONE:          return "NONE";
    case COMMAND_PHASE: return "COMMAND";
    case DATA_PHASE:    return "DATA";
    case STATUS_PHASE:  return "STATUS";
    default:            return "UNKNOWN";
    }
}

/******************************************************************************/

void usb_msd_init(void) 
{
    LOGINFO("initializing the endpoints for the microsd back msd");
    
    scsi_sd_init();
    
    USB0_ENDPT1 = USB_ENDPT_EPCTLDIS | USB_ENDPT_EPRXEN | USB_ENDPT_EPHSHK;
    bdt[BDT_INDEX(1, RX, EVEN)].desc = BDT_DESC(EP1_SIZE, DATA0);
    bdt[BDT_INDEX(1, RX, EVEN)].addr = _ep1_rx[0];
    bdt[BDT_INDEX(1, RX, ODD)].desc  = BDT_DESC(EP1_SIZE, DATA1);
    bdt[BDT_INDEX(1, RX, ODD)].addr  = _ep1_rx[1];
    
    USB0_ENDPT2 = USB_ENDPT_EPCTLDIS | USB_ENDPT_EPTXEN | USB_ENDPT_EPHSHK;
    _ep2_data_toggle = DATA0;
    _ep2_odd_toggle  = EVEN;
    
    _phase = NONE;
}

void usb_msd_bulk_only_reset(void) 
{
    /* if a SCSI IN is being performed flush any buffered data and reset */
    /* TODO is this needed? if we get a bomsr shouldn't a previous cdb failed */
    if (_phase == DATA_PHASE && !CBW_DIRECTION_IN(_cbw)) 
    {
        scsi_sd_data_in_commit();
    }
    /* to reset the interface for the next cbw just set phase to NONE*/
    _phase = NONE;
}

/**** USB ENDPOINT HANDLERS ***************************************************/

/* handler for USB0_ENDPT1 */
void usb_ep1_handler(bdt_t *bd) 
{
    uint8_t buff[EP1_SIZE];
    uint16_t length;
    
    if (BDT_PID(bd->desc) == PID_OUT) 
    {
        length = BDT_DESC_LENGTH(bd->desc);
        memcpy(buff, bd->addr, length);
        bd->desc = BDT_DESC(EP1_SIZE, BDT_DESC_DATA_TOGGLE(bd->desc));
        msd_rx_success(buff, length);
        
    } 
    else 
    {
        LOGWARN("unhandled pid 0x%hx", BDT_PID(bd->desc));
        sput_pid(BDT_PID(bd->desc)); 
        sprint("\n");
        bd->desc = BDT_DESC(EP1_SIZE, BDT_DESC_DATA_TOGGLE(bd->desc));
    }
    USB0_CTL = USB_CTL_USBENSOFEN;
}

/* handler for USB0_ENDPT2 */
/* 
 * tx handler therefore will recieve the TOKDNE interrupt after a token was 
 * successfully transmitted
 */
void usb_ep2_handler(bdt_t *bd) 
{
    switch (BDT_PID(bd->desc)) 
    {
    case PID_IN:
        msd_tx_success(BDT_DESC_LENGTH(bd->desc));
        break;
    
    default:
        LOGWARN("unhandled pid 0xhx", BDT_PID(bd->desc));
        sput_pid(BDT_PID(bd->desc)); 
        sprint("\n");
        break;
    }

    USB0_CTL = USB_CTL_USBENSOFEN;
};

/******************************************************************************/

void begin_transaction(const void *data, uint16_t length) 
{
    ssize_t count;

    if (!is_valid_cbw(data, length, MSD_IMPL_MAX_LUN)) 
    {
        LOGERROR("invalid cbw");
        sxxd(data, length);
        /* signal to the host the cbw is invalid */
        usb_stall_endpoint(MSD_RX_ENDPOINT);
        usb_stall_endpoint(MSD_TX_ENDPOINT);
        send_status(CSW_FAILED, 0);
        return;
    }
    
    _phase = COMMAND_PHASE;
    _cbw = *((const struct usb_msd_cbw *) data);
    _bytes_sent     = 0;
    _bytes_recieved = 0;
    
    count = scsi_sd_begin((const void *) _cbw.CBWCB, _cbw.bCBWCBLength);
    LOGDEBUG("bytes in data phase: 0x%x", count);
    
    if (count < 0) 
    {
        LOGERROR("returned for CDB, sending CSW with failed status");
        /* stall if host is expecting bytes before the csw */
//         if (_cbw.dCBWDataTransferLength > 0) {
//             if (CBW_DIRECTION_IN(_cbw)) {
//                 usb_stall_endpoint(MSD_TX_ENDPOINT);
//             } else {
//                 usb_stall_endpoint(MSD_RX_ENDPOINT);
//             }
//         }
        send_status(CSW_FAILED, 0);
        return;
    }
    
    /* count is the number of bytes the scsi device will be sending or recieving
       to fulfill the CDB */
    _bytes_device = (size_t) count;
    
    /* if we don't have an agreement on the # of bytes in the DATA phase */
    if (_bytes_device != _cbw.dCBWDataTransferLength) 
    {
        /* the device is expecting more data than the host will be sending */
        if (!CBW_DIRECTION_IN(_cbw) && 
                _bytes_device < _cbw.dCBWDataTransferLength) 
        {
            usb_stall_endpoint(MSD_RX_ENDPOINT);
            usb_stall_endpoint(MSD_TX_ENDPOINT);
            send_status(CSW_STATUS_PHASE_ERROR, 0);
            return;
        }
        
        /* device expects to send/recieve data, but host doesn't */
        if (_cbw.dCBWDataTransferLength == 0) 
        {
            usb_stall_endpoint(MSD_TX_ENDPOINT);
            send_status(CSW_STATUS_PHASE_ERROR, 0);
            return;
        }
        
        /* don't worry about it, do the best we can and the dCSWDataResidue 
           will reflect how many bytes actually got processed. */
        LOGINFO("host and device disagree on byte count");
    }
    
    if (_bytes_device == 0) 
    {
        send_status(CSW_SUCCESS, 0);
        return;
    }
    
    _phase = DATA_PHASE;
    if (CBW_DIRECTION_IN(_cbw)) 
    {
        LOGDEBUG("begin sending bytes to host");
        transmit_next();
    }
}

void msd_tx_success(size_t bytes_sent) 
{
    switch (_phase) 
    {
    case DATA_PHASE:
        /* update the number of bytes we have sent so far */
        _bytes_sent += bytes_sent;
        
        if (!CBW_DIRECTION_IN(_cbw)) 
        {
            LOGCRITICAL("CBW with dir OUT TXing in DATA_PHASE");
            usb_stall_endpoint(MSD_RX_ENDPOINT);
            usb_stall_endpoint(MSD_TX_ENDPOINT);
            send_status(CSW_STATUS_PHASE_ERROR, _bytes_sent);
            return;
        }
        
        if (_bytes_sent > _cbw.dCBWDataTransferLength) 
        {
            LOGCRITICAL("don't know how we got here");
            usb_stall_endpoint(MSD_RX_ENDPOINT);
            usb_stall_endpoint(MSD_TX_ENDPOINT);
            send_status(CSW_STATUS_PHASE_ERROR, _bytes_sent);
            return;
        }
        
        /* normal data complete */
        if (_bytes_sent == _cbw.dCBWDataTransferLength) 
        {
            send_status(CSW_SUCCESS, _bytes_sent);
            return;
        }
        
        /* we only tx full packets, unless there is no more data to send. 
           Therefore if we get here the host is expecting more data than we 
           have. */
        if (bytes_sent < EP2_SIZE) 
        {
            /* we could also pad up to the # of bytes the host is expecting */
//             usb_stall_endpoint(MSD_TX_ENDPOINT);
            send_status(CSW_SUCCESS, _bytes_sent);
            return;
        }
        
        /* host is expecting more data and we last sent a full packet */
        transmit_next();
        break;
        
    case STATUS_PHASE: 
        /* status successfully sent */
        _phase = NONE;
        break;
        
    case COMMAND_PHASE:
    case NONE:
        LOGERROR("recieved PID_IN SUCCESS on ep2 while in %s phase", 
            phase2string(_phase));
        break;
    }
}

void msd_rx_success(void *bytes, size_t length) 
{
    ssize_t count;
    switch (_phase) 
    {
    case NONE:
        begin_transaction(bytes, length); /* expected to be a CBW */
        break;
        
    case DATA_PHASE:
        if (CBW_DIRECTION_IN(_cbw)) 
        {
            LOGCRITICAL("RXing in DATA phase while CBW dir is IN");
            usb_stall_endpoint(MSD_RX_ENDPOINT);
            usb_stall_endpoint(MSD_TX_ENDPOINT);
            send_status(CSW_STATUS_PHASE_ERROR, _bytes_recieved);
            return;
        }
        
        /* update how many bytes the host has sent us */
        _bytes_recieved += length;
        
        /* give the bytes to the scsi impl for writing */
        if (scsi_sd_data_in(bytes, length) != 0) 
        {
            LOGERROR("writing bytes to scsi_sd IN");
            /* retrieve the number of bytes successfully written */
            count = scsi_sd_data_in_commit();
            /* we have an error and the host is expecting to send more data */
            if (_bytes_recieved < _cbw.dCBWDataTransferLength) 
            {
                usb_stall_endpoint(MSD_RX_ENDPOINT);
            }
            send_status(CSW_FAILED, (size_t) (count<0? -1*(count+1) : count));
            return;
        }
        
        /* scsi_sd has accepted the bytes */
        
        /* if we have recieved all the data the host has said it will send or
           all the data the CDB expected */
        if (_bytes_recieved == _cbw.dCBWDataTransferLength 
                || _bytes_recieved == _bytes_device) 
        {
            /* tell the scsi impl to write whatever data it has, this will 
               error if it is not block aligned */
            count = scsi_sd_data_in_commit();
            if (count < 0) 
            {
                LOGERROR("failed to commit scsi_sd DATA IN, stalling ep1");
                usb_stall_endpoint(1);
                send_status(CSW_FAILED, (size_t) (-1 * (count + 1)));    
            } 
            else 
            {
                send_status(CSW_SUCCESS, (size_t) count);
            }
            return;
        }
        break;
        
    default:
        LOGERROR("PID_OUT in state: %s", phase2string(_phase));
        sxxd(bytes, length);
        break;
    }
}

void send_status(uint8_t status, size_t processed) 
{
    if (processed != _cbw.dCBWDataTransferLength) 
    {
        LOGINFO("CSW.dCSWDataResidue is not 0: 0x%x", 
            _cbw.dCBWDataTransferLength - processed);
    }
    
    _csw = (struct usb_msd_csw) {
        .dCSWSignature   = htole32(CSW_SIGNATURE),
        .dCSWTag         = htole32(_cbw.dCBWTag),
        .dCSWDataResidue = htole32(_cbw.dCBWDataTransferLength - processed),
        .bCSWStatus      = status
    };
    _phase = STATUS_PHASE;
    ep2_transmit(&_csw, sizeof(_csw));
}


void transmit_next(void) 
{
    ssize_t count;
    void *ptr;
    
    /* check if there are more bytes to send */
    if ((count = scsi_sd_data_out(&ptr, EP2_SIZE)) < 0) 
    {
        LOGERROR("DATA OUT phase");
        /* if we are here that means no packets have been sent to the host or 
           the last packet was full, therefore just check if the host is 
           expecting more */
        if (_bytes_sent < _cbw.dCBWDataTransferLength) 
        {
            usb_stall_endpoint(MSD_TX_ENDPOINT);
        }
        send_status(CSW_FAILED, _bytes_sent);
        return;
    }
    
    LOGDEBUG("`scsi_data_out` returned 0x%x", count);
    
    /* we have no more bytes to send and the last packet was full */
    if (count == 0) 
    {
        if (_bytes_sent < _cbw.dCBWDataTransferLength) 
        {
            usb_stall_endpoint(MSD_TX_ENDPOINT);
        }
        send_status(CSW_SUCCESS, _bytes_sent);
        return;
    }
    
    /* count will be at most EP2_SIZE therefore just make sure the host is 
       expecting that many bytes */
    if (count > (ssize_t) (_cbw.dCBWDataTransferLength - _bytes_sent)) 
    {
        LOGINFO("scsi_sd returned more data than the CBW/host expected");
        count = _cbw.dCBWDataTransferLength - _bytes_sent;
    }
    
    /* queue the data for transmission */
    ep2_transmit(ptr, (size_t) count);
}


void ep2_transmit(const void *data, size_t length) 
{
    LOGDEBUG("%p: %u bytes DATA%d/%s", data, length, _ep2_data_toggle, 
        _ep2_odd_toggle?"ODD":"EVEN");
 
    if (length > EP2_SIZE) { LOGERROR("data too large"); return; }
    
    bdt[BDT_INDEX(2, TX, _ep2_odd_toggle)].addr = (void *) data;
    bdt[BDT_INDEX(2, TX, _ep2_odd_toggle)].desc = BDT_DESC(length, _ep2_data_toggle);
    _ep2_odd_toggle  ^= 1;
    _ep2_data_toggle ^= 1;
}


int is_valid_cbw(const struct usb_msd_cbw *cbw,uint16_t length,uint8_t max_lun)
{
    return  
        /* bdt length of the cbw is correct, check first in case it's < 4 */
        length == CBW_LENGTH                                            && 
        /* cbw signature must match                                       */
        cbw->dCBWSignature == CBW_SIGNATURE                             &&
        /* given the device's max lun, make sure we are valid             */
        (cbw->bCBWLUN & CBW_LUN_MASK) <= max_lun                        &&
        /* the command block's length must be in (0,16]                   */
        (cbw->bCBWCBLength & CBW_CB_LENGTH_MASK) > 0                    && 
        (cbw->bCBWCBLength & CBW_CB_LENGTH_MASK) <= CBW_CB_MAX_LENGTH
    ;
}
