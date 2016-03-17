#ifndef _report_luns_h_
#define _report_luns_h_

#include <stdint.h>

#define REPORT_LUNS_OPCODE (0xa0)
#define REPORT_LUNS_LENGTH (12)

struct report_luns {
    uint8_t  opcode;
    uint8_t  _reserved0;
    uint8_t  select_report;
    uint8_t  _reserved1[3];
    uint32_t allocation_length;
    uint8_t  _reserved2;
    uint8_t  control;
} __attribute__((packed));

struct report_luns_parameter_data {
    uint32_t lun_list_length;
    uint32_t _reserved;
    uint64_t lun_list[];
} __attribute__((packed));

typedef struct report_luns report_luns_t;
typedef struct report_luns_parameter_data report_luns_parameter_data_t;

#endif