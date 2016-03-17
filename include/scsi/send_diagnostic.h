#ifndef _send_diagnostic_h_
#define _send_diagnostic_h_

#include <stdint.h>

/* scsi spc 3r23 p223 6.28 SEND DIAGNOSTIC command */

/* STC - Self Test Code p223 Table 172 */

#define SEND_DIAGNOSTIC_OPCODE                      (0x1d)

/* masks */
#define SEND_DIAGNOSTIC_SELF_TEST_CODE_MASK         (0xe0)
#define SEND_DIAGNOSTIC_PAGE_FORMAT_MASK            (0x10)
#define SEND_DIAGNOSTIC_SELF_TEST_MASK              (0x04)
#define SEND_DIAGNOSTIC_DEVICE_OFFLINE_MASK         (0x02)
#define SEND_DIAGNOSTIC_UNIT_OFFLINE_MASK           (0x01)
/* background self test codes */
#define SEND_DIAGNOSTIC_SELF_TEST_CODE_BG_SHORT     (0x20)
#define SEND_DIAGNOSTIC_SELF_TEST_CODE_BG_EXTENDED  (0x40)
#define SEND_DIAGNOSTIC_SELF_TEST_CODE_BG_ABORT     (0x80)
/* foreground self test codes */
#define SEND_DIAGNOSTIC_SELF_TEST_CODE_FG_SHORT     (0xa0)
#define SEND_DIAGNOSTIC_SELF_TEST_CODE_FG_EXTENDED  (0xc0)


struct send_diagnostic {
    uint8_t  opcode;
    uint8_t  self_test_code;
    uint8_t  _reserved;
    uint16_t parameter_list_length;
    uint8_t  control;
} __attribute__((packed));

typedef struct send_diagnostic send_diagnostic_t;

#endif