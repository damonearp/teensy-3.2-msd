#ifndef _sense_data_h_
#define _sense_data_h_

#include <stdint.h>

/*
 * scsi-spc-3r23.pdf p38 4.5.3 Fixed format sense data 
 */

#define FIXED_FORMAT_SENSE_DATA_LENGTH    (0x12)

/* MASKS */

#define FIXED_FORMAT_SENSE_DATA_SENSE_KEY_MASK (0x0f)

/* 4.5.1 Table 12 */
#define RESPONSE_CODE_CURRENT_FIXED       (0x70)
#define RESPONSE_CODE_DEFERRED_FIXED      (0x71)
#define RESPONSE_CODE_CURRENT_DESCRIPTOR  (0x72)
#define RESPONSE_CODE_DEFERRED_DESCRIPTOR (0x73)
#define RESPONSE_CODE_VENDOR_SPECIFIC     (0x7f)

/* 4.5.6 Table 27 */
#define SENSE_KEY_NO_SENSE                (0x00)
#define SENSE_KEY_RECOVERED_ERROR         (0x01)
#define SENSE_KEY_NOT_READY               (0x02)
#define SENSE_KEY_MEDIUM_ERROR            (0x03)
#define SENSE_KEY_HARDWARE_ERROR          (0x04)
#define SENSE_KEY_ILLEGAL_REQUEST         (0x05)
#define SENSE_KEY_UNIT_ATTENTION          (0x06)
#define SENSE_KEY_DATA_PROTECT            (0x07)
#define SENSE_KEY_BLANK_CHECK             (0x08)
#define SENSE_KEY_VENDOR_SPECIFIC         (0x09)
#define SENSE_KEY_COPY_ABORTED            (0x0a)
#define SENSE_KEY_ABORTED_COMMAND         (0x0b)
#define SENSE_KEY_VOLUME_OVERFLOW         (0x0d)
#define SENSE_KEY_MISCOMPARE              (0x0e)


/* For ASC and ASCQ assignments see p42 4.5.6 Table 28 */

#define ASC_ASCQ_PERIPHERAL_DEVICE_WRITE_FAULT      (0x0300)
#define ASC_ASCQ_LUN_NOT_READY                      (0x0400)
#define ASC_ASCQ_UNRECOVERD_READ_ERROR              (0x1100)
#define ASC_ASCQ_INVALID_COMMAND                    (0x2000)
#define ASC_ASCQ_LBA_OUT_OF_RANGE                   (0x2100)
#define ASC_ASCQ_LUN_NOT_SUPPORTED                  (0x2500)
#define ASC_ASCQ_INVALID_FIELD_IN_CDB               (0x2400)
#define ASC_ASCQ_NOT_READY_MEDIUM_MAY_HAVE_CHANGED  (0x2800)
#define ASC_ASCQ_FORMAT_COMMAND_FAILED              (0x3101)
#define ASC_ASCQ_MEDIUM_NOT_PRESENT                 (0x3a00)


#define FIXED_FORMAT_SENSE_DATA_DEFAULT                                         \
    (struct fixed_format_sense_data) { 0,0,0,0,0x0a,0,{{0,0}},0,0 }

/* Macro Functions */

/* 
 * given the length of the Fixed Format Sense Data being sent, calculate the 
 * `additional_length` field's value
 */
#define FixedFormatAdditionalLength(len) (len - 7)

/*
 * combine the `valid` field with the `response_code` 
 */
#define FixedFormatResponseCode(v, rc) \
    (((v & 0x01) << 7) | (rc & 0x7f))
    
#define FixedFormatSenseKey(fm, eom, ili, sk) \
    (((fm & 0x01)<<7) | ((eom & 0x01)<<6) | ((ili & 0x01)<<5) | (sk & 0x0f))

#define FixedFormatSenseKeySpecific(sksv, sks) \
    ((((uin16_t) sksv & 0x0001) << 15) | ((uint16_t) sks & 0x7fff))
    
#define FixedFormatSenseData(v,rc,fm,eom,ili,sk,info,len,csi,asc,ascq,fruc,sks) \
    (struct fixed_format_sense_data) {                                          \
        .response_code = FixedFormatResponseCode(v, rc),                        \
        ._obsolete = 0,                                                         \
        .sense_key = FixedFormatSetSenseKey(fm,eom,ili,sk),                     \
        .information = info,                                                    \
        .additional_length = FixedFormatAdditionalLength(len),                  \
        .csi = csi,                                                             \
        .asc = asc,                                                             \
        .ascq = ascq,                                                           \
        .fruc = fruc,                                                           \
        .sks = FixedFormatSenseKeySpecific(sksv, sks)                           \
    }

#define FixedFormatSenseDataClear(ffsd) \
    (ffsd) = FIXED_FORMAT_SENSE_DATA_DEFAULT
    
struct fixed_format_sense_data {
    uint8_t  response_code;                 /* Valid, Response Code           */
    uint8_t  _obsolete;
    uint8_t  sense_key;                     /* Filemark, EOM, ILI, Sense Key  */
    uint32_t information;
    uint8_t  additional_length;             
    uint32_t csi;                           /* command specific information   */
    union {
        struct {
            uint8_t  asc;                   /* additional sense command       */
            uint8_t  ascq;                  /* asc qualifier                  */
        };
        uint16_t asc_ascq;
    };
    uint8_t  fruc;                          /* field_replacable_unit_code     */
    uint16_t sks;                           /* sksv, Sense Key Specific       */
}__attribute__((packed));


typedef struct fixed_format_sense_data fixed_format_sense_data_t;


#endif