/* simple c interface for the c++ Sd2Card class */

#include <kinetis.h>

#include "sd.h"
#include "Sd2Card.h"
#include "SPI.h"
#include "serialize.h" 

#define CHIP_SELECT_PIN 4 /* teensy 3.2 */

/* NOTE
 * `sd_init()` needs to be called after hardware intialization. First attempt 
 * called the function right after calling `usb_init()` in pins_teensy.c, this
 * led to a weird bug where Sd2Card.type_ would be reset to 0 the moment 
 * `usb_isr()` was called. The odd thing was this field was the only one 
 * changed. Once `sd_init()` was moved to once the usb driver recieves a 
 * SET CONFIGURATION setup command everything worked fine.
 */

namespace {
    Sd2Card _card;
}

int sd_init(void) 
{
    if (!_card.init(SPI_FULL_SPEED, CHIP_SELECT_PIN)) 
    {
        LOGERROR("cannot find an sd card");
        return -1;
    }
    return 0;
}

uint32_t sd_max_lba(void) 
{
    return _card.cardSize();
}

int sd_read_block(void *dest, uint32_t lba) 
{
    if (!_card.readBlock(lba, (uint8_t *) dest)) 
    {
        LOGERROR("failed to read block lba 0x%08x code: %hu data: %hu", 
            lba, _card.errorCode(), _card.errorData());
        return -1;
    }
    return 0;
}

int sd_write_block(uint32_t lba, const void *src) 
{
    if (!_card.writeBlock(lba, (const uint8_t *) src)) 
    {
        LOGERROR("failed to write block lba 0x%08x code: %hu data: %hu", 
            lba, _card.errorCode(), _card.errorData());
        return -1;
    }
    return 0;
}
