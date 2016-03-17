#ifndef _usb_bdt_h
#define _usb_bdt_h

#include <stdint.h>
#include "usb_desc.h" /* for NUM_ENDPOINTS */

#define USB_BDT_LENGTH (NUM_ENDPOINTS * 4)

#define BDT_OWN		0x80           /* Ownership belongs to the USB-FS */
#define BDT_DATA1	0x40
#define BDT_DATA0	0x00           
#define BDT_DTS		0x08           /* Data Toggle Synchronization */
#define BDT_STALL	0x04

#define BDT_INDEX(endpoint, tx, odd) (((endpoint) << 2) | ((tx) << 1) | (odd))

#define BDT_PID(n)	(((n) >> 2) & 0x0f)

#define BDT_DESC(count, data)	                                               \
    (BDT_OWN | BDT_DTS | ((data) ? BDT_DATA1 : BDT_DATA0) | ((count) << 16))
    
#define BDT_DESC_DATA_TOGGLE(desc) ((desc & BDT_DATA1) >> 2)

#define BDT_DESC_LENGTH(desc) ((desc >> 16) & 0x03ff)

typedef struct {
    uint32_t desc;
    void *addr;
} bdt_t;

extern bdt_t bdt[];

#endif