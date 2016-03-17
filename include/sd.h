#ifndef _sd_card_h_
#define _sd_card_h_

#ifdef __cplusplus
extern "C" {
#endif
    
#include <stdint.h>
    
#define SD_BLOCK_SIZE (512)
    
int sd_init(void);    
uint32_t sd_max_lba(void);
int sd_read_block(void *dest, uint32_t lba);
int sd_write_block(uint32_t lba, const void *src);

#ifdef __cplusplus
}
#endif

#endif
