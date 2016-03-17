#ifndef _request_sense_h_
#define _request_sense_h_

#include <stdint.h>

/*
 * scsi-spc-3r23.pdf p221 6.27 REQUEST SENSE command 
 */

#define REQUEST_SENSE_LENGTH    (0x06)

/* MASKS */
#define REQUEST_SENSE_DESC_MASK (0x01)

/* COMMAND VALUES */
#define REQUEST_SENSE_OPCODE    (0x03)


struct request_sense {
    uint8_t opcode;
    uint8_t desc;
    uint16_t _reserved;
    uint8_t allocation_length;
    uint8_t control;
}__attribute__((packed));


typedef struct request_sense request_sense_t;


#endif