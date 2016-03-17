#ifndef _scsi_sd_h_
#define _scsi_sd_h_

#include <unistd.h>
#include "scsi/scsi.h"

#ifdef __cplusplus
extern "C" {
#endif
    
/* 
 * sets up communication with the sd card and intializing SCSI structures. 
 * Returns 0 on success, error otherwise. 
 */    
int scsi_sd_init(void);

/*
 * Sets up state information for the new CDB. Returning the number of bytes, 
 * expected to be transfered in the DATA PHASE. Returns < 0 if there is an 
 * error.
 */
ssize_t scsi_sd_begin(const void *cdb, size_t cdblen);

/*
 * Returns the number of valid bytes that `ptr` will point to (<= maxlen). If
 * there is no more data in the OUT stage `ptr` will be set to NULL and 0 will
 * be returned. If return value is < 0 then an error occurred, and scsi sense 
 * data was updated.
 */
ssize_t scsi_sd_data_out(void **ptr, size_t maxlen);

/*
 */
int scsi_sd_data_in(const void *ptr, size_t length);

/*
 * Required to be called if writing data to the sd card
 * 
 * flushes any completed blocks in the buffer to the sd card and returns the 
 * number of bytes, provided by `scs_sd_data_in`, successfully written to the sd
 * card. If an error occurs while writing remaining data then it returns
 * -(# bytes successfully written + 1). If there is an unfull block of data in
 * the buffer, the remainder will not be written and return value will not 
 * include that count, furthermore this routine clears the buffer losing any 
 * unwritten data.
 */
ssize_t scsi_sd_data_in_commit(void);

#ifdef __cplusplus
}
#endif
#endif
