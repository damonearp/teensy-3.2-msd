#ifndef _usb_msd_h_
#define _usb_msd_h_

#include <stdint.h>
#include "serialize.h" 

/* implementation details for our msd */
#define MSD_RX_ENDPOINT (1)
#define MSD_RX_ENDPOINT_SIZE EP1_SIZE
#define MSD_TX_ENDPOINT (2)
#define MSD_TX_ENDPOINT_SIZE EP2_SIZE
#define MSD_IMPL_MAX_LUN     (0)
/**************************************/

#define CBW_LENGTH              (0x1f)
#define CBW_SIGNATURE           (0x43425355)
#define CBW_FLAGS_IN            (0x80)
#define CBW_FLAGS_OUT           (0x00)
#define CBW_CB_MAX_LENGTH       (0x10)
#define CBW_LUN_MASK            (0x0f)
#define CBW_CB_LENGTH_MASK      (0x1f)

#define CBW_DIRECTION_IN(cbw)   (((cbw).bmCBWFlags & CBW_FLAGS_IN) != 0)

#define CSW_LENGTH              (0x0d)
#define CSW_SIGNATURE           (0x53425355)
#define CSW_SUCCESS             (0x00)
#define CSW_FAILED              (0x01)
#define CSW_STATUS_PHASE_ERROR  (0x02)


struct usb_msd_cbw {
    uint32_t    dCBWSignature;
    uint32_t    dCBWTag;
    uint32_t    dCBWDataTransferLength;
    uint8_t     bmCBWFlags;
    uint8_t     bCBWLUN;
    uint8_t     bCBWCBLength;
    uint8_t     CBWCB[CBW_CB_MAX_LENGTH];
} __attribute__((packed));


struct usb_msd_csw {
    uint32_t    dCSWSignature;
    uint32_t    dCSWTag;
    uint32_t    dCSWDataResidue;
    uint8_t     bCSWStatus;
} __attribute__((packed));

/******************************************************************************/

void usb_msd_init(void);
void usb_msd_bulk_only_reset(void);


#endif
