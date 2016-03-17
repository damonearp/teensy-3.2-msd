#ifndef _mode_sense_h
#define _mode_sense_h

#include <stdint.h>

/*
 * scsi-spc-3r23.pdf p164 6.9 MODE SENSE(6) command 
 * scsi-spc-3r23.pdf p167 6.10 MODE SENSE(10) command 
 */

#define MODE_SENSE6_LENGTH          (0x06)
#define MODE_SENSE10_LENGTH         (0x0a)

/* MASKS */
#define MODE_SENSE6_DBD_MASK        (0x08)
#define MODE_SENSE6_PC_MASK         (0xc0)
#define MODE_SENSE6_PAGE_CODE_MASK  (0x3f)
#define MODE_SENSE10_LLBAA_MASK     (0x10)
#define MODE_SENSE10_DBD_MASK       (0x08)
#define MODE_SENSE10_PC_MASK        (0xc0)
#define MODE_SENSE10_PAGE_CODE_MASK (0x3f)

/* COMMAND VALUES */
#define MODE_SENSE6_OPCODE          (0x1a)
#define MODE_SENSE10_OPCODE         (0x5a)

#define MODE_SENSE_PAGE_CODE_CACHING    (0x08)
#define MODE_SENSE_PAGE_CODE_RETURN_ALL (0x3f)

/* p165 Table 98 Page control (pc) field*/
#define MODE_SENSE_PC_CURRENT       (0x00)
#define MODE_SENSE_PC_CHANGEABLE    (0x01)
#define MODE_SENSE_PC_DEFAULT       (0x10)
#define MODE_SENSE_PC_SAVED         (0x11)


struct mode_sense6 {
    uint8_t opcode;
    uint8_t dbd;                /* disable block descriptors */
    uint8_t pc_page_code;       /* page control and page code*/
    uint8_t subpage_code;
    uint8_t allocation_length;
    uint8_t control;
} __attribute__((packed));


struct mode_sense10 {
    uint8_t  opcode;
    uint8_t  dbd;
    uint8_t  pc_page_code;
    uint8_t  subpage_code;
    uint8_t  _reserved[3];
    uint16_t allocation_length;
    uint8_t  control;
} __attribute__((packed));


typedef struct mode_sense6      mode_sense6_t;
typedef struct mode_sense10     mode_sense10_t;

#endif
