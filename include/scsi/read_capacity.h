#ifndef _read_capacity_h_
#define _read_capacity_h_

#include <stdint.h>

/*
 * scsi-sbc-344r0.pdf p53 5.10 READ CAPACITY (10) command
 */

#define READ_CAPACITY10_LENGTH      (0x0a)
/* MASKS */
#define READ_CAPACITY10_PMI_MASK    (0x01)
/* COMMAND VALUES */
#define READ_CAPACITY10_OPCODE      (0x25)


struct read_capacity10 {
    uint8_t     opcode;
    uint8_t     _reserved0;
    uint32_t    lba;                /* logical block address big-endian       */
    uint16_t    _reserved1;
    uint8_t     pmi;                /* partial medium indicator               */
    uint8_t     control;
} __attribute__((packed));


struct read_capacity10_data {
    uint32_t lba;                   /* the lba information was requested for */
    uint32_t block_length;          /* number of bytes of data in the lba    */
} __attribute__((packed));


typedef struct read_capacity10      read_capacity10_t;
typedef struct read_capacity10_data read_capacity10_data_t;

#endif