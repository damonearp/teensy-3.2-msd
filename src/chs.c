#include <stdint.h>
#include <limits.h>
#include "chs.h"

int lba2chslimits(struct chslimits *info, uint32_t max_lba) 
{
    uint8_t spt = CHS_SECTORS_PER_TRACK_DEFAULT;
    uint8_t hpc = CHS_HEADS_PER_CYLINDER;
    uint32_t cc;
    
    while ((cc = max_lba / (hpc * spt)) > USHRT_MAX) 
    {
        hpc += CHS_HEADS_PER_CYLINDER;
    }
    
    *info = (struct chslimits) {
        .head_count         = hpc,
        .track_sector_count = spt,
        .cylinder_count     = cc
    };
    
    return 0;
}
