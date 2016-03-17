#ifndef _inquiry_h_
#define _inquiry_h_

#include <stdint.h>

/*
 * scsi-spc-3r23.pdf p142 6.6 INQUIRY command 
 */

#define INQUIRY_LENGTH                    (0x06)

/* MASKS */
#define INQUIRY_EVPD_MASK                 (0x80)

/* COMMAND VALUES */
#define INQUIRY_OPCODE                    (0x12)

/* DATA VALUES */
#define INQUIRY_DATA_PDT_DABD             (0x00) /* direct access block dev   */
#define INQUIRY_DATA_PDT_CDD              (0x05) /* cd/dvd device             */
#define INQUIRY_DATA_PDT_OMD              (0x07) /* optical memory device     */
#define INQUIRY_DATA_PDT_RBC              (0x0e) /* reduced block cmd dev     */
#define INQUIRY_DATA_PQ_CONNECTED         (0x00) /* periph dev is connected   */
#define INQUIRY_DATA_RMB_REMOVABLE        (0x80) 
#define INQUIRY_DATA_VERSION_NON_STANDARD (0x00)
#define INQUIRY_DATA_RDF_STANDARD         (0x02) /* conforms to scsi standard */

/* calculate the additional_length field of struct inquiry_data */
#define INQUIRY_DATA_ADDITIONAL_LENGTH(resplen) (resplen - 5)


struct inquiry {
    uint8_t     opcode;
    uint8_t     evpd;
    uint8_t     page_code;
    uint16_t    allocation_length;
    uint8_t     control;
} __attribute__((packed));


struct inquiry_data {
    uint8_t peripheral;             /* peripheral device type and qualifier   */
    uint8_t removable;              /* removable device flag                  */
    uint8_t version;
    uint8_t response_data_format;   /* flags including the rdf                */
    uint8_t additional_length;      /* (response_length - 5)                  */
    uint8_t flags[3];               /* many flags, but all 0 under usb msd    */
    char    vendor_id[8];
    char    product_id[16];
    char    product_revision[4];
} __attribute__((packed));


typedef struct inquiry      inquiry_t;
typedef struct inquiry_data inquiry_data_t;


#endif