#ifndef _chs_h_
#define _chs_h_

#ifdef __cplusplus
extern "C" {
#endif
    
/* Reference: 
 *      https://en.wikipedia.org/wiki/Logical_block_addressing#CHS_conversion 
 * 
 * LBA - Logical block address
 * HPC - Heads per Cylinder
 * SPT - Sectors per Track
 * C   - Cylinder
 * H   - Head
 * S   - Sector
 * 
 * 
 * LBA = (C × HPC + H) × SPT + (S - 1)
 * C   = LBA / (HPC * SPT)
 * H   = (LBA / SPT) % HPC
 * S   = (LBA % SPT) + 1 
 */

/* with 28bit lba's suggested maxes, source wikipedia (feel free to ignore) */
#define CHS_HEADS_PER_CYLINDER          (16)
#define CHS_SECTORS_PER_TRACK_DEFAULT   (63)


/* LBA = (C × HPC + H) × SPT + (S - 1) */
#define CHS2LBA(c,h,s,hpc,spt) ((((c * hpc) + h) * spt) + (s - 1))


struct chslimits {
    uint8_t  head_count;            /* number of heads per cylinder         */
    uint8_t  track_sector_count;    /* number of selectors per track        */
    uint16_t cylinder_count;        /* number of cylinders in the storage   */
};
typedef struct chslimits chslimits_t;

/* 
 * find viable chs values to describe a disk with `lba_count` number of blocks. 
 * The CHS values will assume the original block size is the nubmer of bytes per 
 * sector.
 */ 
int lba2chslimits(struct chslimits *info, uint32_t lba_count);

#ifdef __cplusplus
}
#endif
#endif