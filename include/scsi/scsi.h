#ifndef _scsi_h_
#define _scsi_h_

/* SENSE and MODE data */
#include "sense_data.h"
#include "mode_parameter.h"
#include "mode_page.h"

/* SCSI Command Descriptor Blocks */
#include "inquiry.h"
#include "load_unload.h"
#include "mode_sense.h"
#include "prevent_allow_medium_removal.h"
#include "read.h"
#include "read_capacity.h"
#include "read_format_capacities.h"
#include "report_luns.h"
#include "request_sense.h"
#include "send_diagnostic.h"
#include "test_unit_ready.h"
#include "write.h"

/* currently there is no header file for this command */
#define FORMAT_UNIT_OPCODE          (0x04)

/* spc 3r23 p28 4.3.4.1*/
/* cdb opcodes are composed of a 3 byte group code and a 5 byte command code */
#define COMMAND_CODE_MASK           (0x1f)
#define GROUP_CODE_MASK             (0xe0)
#define GROUP_CODE_SHIFT            (5)
#define GROUP_CODE_CDB6             (0x00)
#define GROUP_CODE_CDB10            (0x20)
#define GROUP_CODE_CDB10_2          (0x40)
#define GROUP_CODE_RESERVED         (0x60) /* variable length cdb */
#define GROUP_CODE_CDB16            (0x80)
#define GROUP_CODE_CDB12            (0xa0)
#define GROUP_CODE_VENDOR           (0xc0)
#define GROUP_CODE_VENDOR_2         (0xe0)

#define CDB6_ALLOCATION_LENGTH_MAX  (0xff)


/* SCSI Status Codes */
enum scsi_status {
    GOOD                        = 0x00, /* task completed successfully        */
    CHECK_CONDITION             = 0x02, /* use REQUEST SENSE                  */
    CONDITION_MET               = 0x04, /* on success of pre-fetch cdb        */
    BUSY                        = 0x08, /* cannot complete at this time       */
    INTERMEDIATE                = 0x10, /* obsolete                           */
    INTERMEDIATE_CONDITION_MET  = 0x14, /* obsolete                           */
    RESERVATION_CONFLICT        = 0x18, /* attempt to access reserved LUN     */
    COMMAND_TERMINATED          = 0x22, /* obsolete                           */
    TASK_SET_FULL               = 0x28, /* LUN lacks resources to accept      */
    ACA_ACTIVE                  = 0x30, /* auto-contingent allegiance condi   */
    TASK_ABORTED                = 0x40  /* TAS control bit set to 1           */
};


struct scsi_command_descriptor_block {
    uint8_t opcode;
};

typedef enum scsi_status scsi_status_t;
typedef struct scsi_command_descriptor_block scsi_cdb_t;

#endif
