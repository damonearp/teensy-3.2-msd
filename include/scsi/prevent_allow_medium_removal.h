#ifndef _prevent_allow_medium_removal_h_
#define _prevent_allow_medium_removal_h_

#include <stdint.h>

#define PREVENT_ALLOW_MEDIUM_REMOVAL_LENGTH (0x06)
#define PREVENT_ALLOW_MEDIUM_REMOVAL_OPCODE (0x1e)

/* 
 * if 01b then prevent removal by Data Transport Element, if 10b prevent 
 * removal by Medium Changer
 */
#define PREVENT_ALLOW_MEDIUM_REMOVAL_PREVENT_DTE     (0x01)
#define PREVENT_ALLOW_MEDIUM_REMOVAL_PREVENT_CHANGER (0x02)

#define PREVENT_ALLOW_MEDIUM_REMOVAL_PREVENT_MASK    (0x03)

struct prevent_allow_medium_removal {
    uint8_t opcode;
    uint8_t _reserved[3];
    uint8_t prevent;
    uint8_t control;
} __attribute__((packed));

typedef struct prevent_allow_medium_removal prevent_allow_medium_removal_t;

#endif