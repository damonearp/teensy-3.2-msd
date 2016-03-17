#ifndef _read_h_
#define _read_h_

#include <stdint.h>

#define READ6_LENGTH (0x06)
#define READ6_OPCODE (0x08)

#define READ10_LENGTH (0x0a)
#define READ10_OPCODE (0x28)

#define READ6_LBA_MASK              (0x1fffff00)
#define READ6_LBA_SHIFT             (2)
#define READ6_TRANSFER_LENGTH_MASK  (0x000000ff)

struct read6 {
    uint8_t opcode;
    union {
        struct {
            uint8_t lba[3];
            uint8_t transfer_length;
        };
        uint32_t lba_transfer_length;
    };
    uint8_t control;
} __attribute__((packed));

struct read10 {
    uint8_t opcode;
    uint8_t flags;
    uint32_t lba;
    uint8_t group;
    uint16_t transfer_length;
    uint8_t control;
} __attribute__((packed));

typedef struct read6 read6_t;
typedef struct read10 read10_t;

#endif