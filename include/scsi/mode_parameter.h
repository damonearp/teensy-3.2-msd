#ifndef _mode_parameter_h_
#define _mode_parameter_h_

#include <stdint.h>

/* scsi spc v3r23 p279 7.4*/

/* GLOSSARY
 * 
 * BD           Block Descriptor
 * DABD         Direct-Access Block Device
 * DSP          Device Specific Parameter (field of the mode parameter header)
 * MPBD         Mode Parameter Block Descriptor
 * MPH          Mode Parameter Header
 * 
 */

#define MODE_PARAMETER_HEADER6_LENGTH  (0x04)
#define MODE_PARAMETER_HEADER10_LENGTH (0x08)
#define MODE_PARAMETER_BLOCK_DESCRIPTOR_SHORT_LENGTH (0x08)
#define MODE_PARAMETER_BLOCK_DESCRIPTOR_LONG_LENGTH  (0x10)


#define MODE_PARAMETER_HEADER10_LONGLBA  (0x01)


#define MODE_PARAMETER_BLOCK_DESCRIPTOR_SHORT_LENGTH_MASK   (0x00ffffff)

/* Direct-Access Block Device DEVICE SPECIFIC PARAMETER field values*/ 
#define MODE_PARAMETER_DSP_DABD_WP     (0x80) /* write protected */
#define MODE_PARAMETER_DSP_DABD_DPOFUA (0x10) /* DPO and FUA supported */

/* for direct-access block devices the `medium_type` is 0 (sbc 344r0 6.3.1) */
#define MODE_PARAMETER_MEDIUM_TYPE_DABD (0x00)

/* p279 7.4.3 Table 239 */
struct mode_parameter_header6 {
    uint8_t mode_data_length;           /* mode data length */
    uint8_t medium_type;                /* medium type */
    uint8_t device_specific_parameter;  /* device specific parameter */
    uint8_t block_descriptor_length;    /* block descriptor length */
}__attribute__((packed));

/* table 240 */
struct mode_parameter_header10 {
    uint16_t mode_data_length;           /* mode data length */
    uint8_t  medium_type;                /* medium type */
    uint8_t  device_specific_parameter;  /* device specific parameter */
    uint8_t  longlba;
    uint8_t  _reserved;
    uint16_t  block_descriptor_length;    /* block descriptor length */
}__attribute__((packed));

typedef struct mode_parameter_header6                mode_parameter_header6_t;
typedef struct mode_parameter_header10               mode_parameter_header10_t;

#endif
