#ifndef _LOAD_UNLOAD_H_
#define _LOAD_UNLOAD_H_

#include <stdint.h>

/* scsi ssc-3 p75 7.2 LOAD UNLOAD command */

#define LOAD_UNLOAD_OPCODE (0x1b)
#define LOAD_UNLOAD_LENGTH (6)

#define HOLD_MASK   (0x08)
#define HOLD_SHIFT  (3)
#define EOT_MASK    (0x04)
#define EOT_SHIFT   (2)
#define RETEN_MASK  (0x02)
#define RETEN_SHIFT (1)
#define LOAD_MASK   (0x01)


struct load_unload {
    uint8_t  opcode;
    uint8_t  immed;
    uint16_t _reserved;
    uint8_t  hold_eot_reten_load;
    uint8_t  control;
} __attribute__((packed));

typedef struct load_unload load_unload_t;

#endif
