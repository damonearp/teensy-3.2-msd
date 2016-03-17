#ifndef _mode_page_h_
#define _mode_page_h_

#include <stdint.h>

#define MODE_PAGE_0_PAGE_CODE_MASK  (0x3f)
#define MODE_PAGE_0_PS              (0x80)
#define MODE_PAGE_0_SPF             (0x40)  /* subpage format flag */

#define FLEXIBLE_DISK_PAGE_CODE     (0x05)
#define FLEXIBLE_DISK_PAGE_LENGTH   (0x1e)

/* scsi-spc-3r23 p282 7.4.5 Table 242 */

struct mode_page_0 {
    uint8_t page_code;                  /* sp:1, spf:1, page_code:6           */
    uint8_t page_length;                /* n - 1                              */
} __attribute__((packed));

/* usb mass storage ufi10 p26 4.5.5 Table 19 */
struct mode_page_flexible_disk {
    uint8_t  page_code;
    uint8_t  page_length;               /* n - 1 */
    uint16_t transfer_rate;
    uint8_t  head_count;                /* # of heads for this device         */
    uint8_t  track_sector_count;        /* # of sectors per track             */
    uint16_t sector_byte_count;         /* # of bytes per sector              */
    uint16_t cylinder_count;            /* # of cylinders in the device       */
    uint8_t  _reserved[22];       /* ufi motor on/off speed and rotation info */
} __attribute__((packed));

typedef struct mode_page_flexible_disk mode_page_flexible_disk_t;

#endif