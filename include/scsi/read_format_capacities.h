#ifndef _READ_FORMAT_CAPACITIES_H_
#define _READ_FORMAT_CAPACITIES_H_

#include <stdint.h>

/* defined in scsi mmc - windows requires for some reason */

#define READ_FORMAT_CAPACITIES_LENGTH (0x0a)
#define READ_FORMAT_CAPACITIES_OPCODE (0x23)

#define CAPACITY_LIST_LENGTH_MASK (0x000000ff)
#define CAPACITY_DESCRIPTOR_DESCRIPTOR_TYPE_MASK  (0x03000000)
#define CAPAICTY_DESCRIPTOR_DESCRIPTOR_TYPE_SHIFT (24)
#define CAPACITY_DESCRIPTOR_BLOCK_LENGTH_MASK     (0x00ffffff)

#define DESCRIPTOR_TYPE_MASK        (0x03)

#define DESCRIPTOR_TYPE_RESERVED    (0)
#define DESCRIPTOR_TYPE_UNFORMATTED (1)
#define DESCRIPTOR_TYPE_FORMATTED   (2)
#define DESCRIPTOR_TYPE_NO_MEDIA    (3)

#define CapacityDescriptorTypeAndLength(dt, blen) (                 \
    (((dt) & 0x03) << CAPAICTY_DESCRIPTOR_DESCRIPTOR_TYPE_SHIFT) |  \
        ((blen) & CAPACITY_DESCRIPTOR_BLOCK_LENGTH_MASK)            \
)

#define CapacityListLength(count)                                   \
    (((count) * sizeof(struct capacity_descriptor)) & 0xff)


struct read_format_capacities {
    uint8_t  opcode;
    uint8_t  _reserved[6];
    uint16_t allocation_length;
    uint8_t  control;
} __attribute__((packed));


struct capacity_descriptor {
    uint32_t number_of_blocks;
    uint32_t descriptor_type_block_length;
} __attribute__((packed));

struct capacity_list_header {
    uint8_t _reserved[3];
    uint8_t capacity_list_length;
} __attribute__((packed));

typedef struct read_format_capacities read_format_capacities_t;
typedef struct capacity_list_header capacity_list_header_t;
typedef struct capacity_descriptor capacity_descriptor_t;


#endif
