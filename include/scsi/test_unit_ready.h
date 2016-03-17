#ifndef _test_unit_ready_h_
#define _test_unit_ready_h_

#include <stdint.h>

/*
 * scsi-spc-3r23.pdf p232 6.33 TEST UNIT READY command
 */

#define TEST_UNIT_READY_LENGTH      (0x06)
/* COMMAND VALUES */
#define TEST_UNIT_READY_OPCODE      (0x00)

struct test_unit_ready {
    uint8_t     opcode;
    uint32_t    _reserved;
    uint8_t     control;
} __attribute__((packed));


typedef struct test_unit_ready test_unit_ready_t;


#endif