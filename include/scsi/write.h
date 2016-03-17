#ifndef _write_h_
#define _write_h_

#include <stdint.h>

#define WRITE6_LENGTH   (0x6)
#define WRITE6_OPCODE   (0x0a)

#define WRITE10_LENGTH  (0xa)
#define WRITE10_OPCODE  (0x2a)

#define WRITE6_LBA_MASK (0x1fffff00)
#define WRITE6_LBA_SHIFT (2)

struct write6 {
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

struct write10 {
    uint8_t  opcode;
    uint8_t  flags;
    uint32_t lba;
    uint8_t  group;
    uint16_t transfer_length;
    uint8_t  control;
}__attribute__((packed));

typedef struct write6  write6_t;
typedef struct write10 write10_t;

#endif
