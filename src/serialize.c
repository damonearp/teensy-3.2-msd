#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include "HardwareSerial.h"
#include "usb_dev.h"
#include "kinetis.h"

#define PID_OUT     (0x01)
#define PID_IN      (0x09)
#define PID_SOF     (0x05)
#define PID_SETUP   (0x0d)
#define PID_DATA0   (0x03)
#define PID_DATA1   (0x0b)
#define PID_DATA2   (0x07)
#define PID_MDATA   (0x0f)
#define PID_ACK     (0x02)
#define PID_NAK     (0x0a)
#define PID_STALL   (0x0e)
#define PID_NYET    (0x06)

#define GET_STATUS              (0x00)
#define CLEAR_FEATURE           (0x01)
#define SET_FEATURE             (0x03)
#define SET_ADDRESS             (0x05)
#define GET_DESCRIPTOR          (0x06)
#define SET_DESCRIPTOR          (0x07)
#define GET_CONFIGURATION       (0x08)
#define SET_CONFIGURATION       (0x09)
#define GET_INTERFACE           (0x0a)
#define SET_INTERFACE           (0x0b)
#define SYNC_FRAME              (0x0c)
#define SET_SEL                 (0x30)
#define SET_ISOCHRONOUS_DELAY   (0x31)

// USB Mass Storage Class bRequest
#define GET_MAX_LUN             (0xfe)
#define BOMS_RESET              (0xff)

// USB bmRequestType parts
// bit 7        X0000000
#define RT_OUT          (0x00)
#define RT_IN           (0x80)
// bits 6..5    0XX00000
#define RT_STANDARD     (0x00)
#define RT_CLASS        (0x20)
#define RT_VENDOR       (0x40)
// bits 4..0    000XXXXX
#define RT_DEVICE       (0x00)
#define RT_INTERFACE    (0x01)
#define RT_ENDPOINT     (0x02)
#define RT_OTHER        (0x03)

#if defined(DEBUG)

void serial_printf(const char *fmt, ...) {
    va_list ap;
    int count;
    char fmtd[256];
    
    va_start(ap, fmt);
    count = vsnprintf(fmtd, sizeof(fmtd), fmt, ap);
    va_end(ap);
    
    serial_print(fmtd);
    if (count >= (int) sizeof(fmtd)) {
        serial_print("...");
    }
}

/* print recognizable ASCII characters after each line */
static inline
void serial_xxd_print_visible(const uint8_t *bytes, uint8_t length) {
    int i;
    for (i = 0; i < length; i++) {
        if (i != 0 && (i % 8) == 0) { serial_putchar(' '); }
        serial_putchar(bytes[i] >= 0x20 && bytes[i] <= 0x7e? bytes[i] : '.');
    }
}

void serial_xxd(const void *bytes, uint16_t length) {
    const uint8_t *byte;
    uint16_t count;
    uint8_t line[16];
    int i;
    uint8_t remainder;
    
    serial_print("0000  ");
    for (byte = bytes, count = 0; count < length; byte++, count++) {
        if (count != 0) {
            if ((count % 16) == 0) {
                serial_print("  ");
                serial_xxd_print_visible(line, 16);
                serial_print("\n");                 /* every 16 bytes '\n'    */
                serial_phex16(count);
                serial_print("  ");
            } else if ((count % 8) == 0) {      
                serial_print("  ");                 /* 2 spaces mid line      */
            } else {
                serial_print(" ");                  /* 1 space between each   */
            }
        }
        line[count % 16] = *byte;
        serial_phex(*byte);
    }
    
    if (count > 0) {
        remainder = count % 16;
        if (remainder == 0) { remainder = 16; }        /* if */
        for (i = 0; i < (16 - remainder); i++) {       /* pad with spaces */
            if (i > 0 && (i % 8) == 0) { serial_putchar(' '); }
            serial_print("   ");
        }
        serial_print("  ");
        serial_xxd_print_visible(line, remainder);
    }
    serial_print("\n");
}


void sput_istat(uint8_t istat) {
#define UNSET "      "
    // 7     6      5      4     3      2      1     0
    // STALL ATTACH RESUME SLEEP TOKDNE SOFTOK ERROR USBRST
    const char *flag;
    serial_print("istat [ ");
    flag = (istat & 0x80)? "STALL " : UNSET;
    serial_print(flag);
    
    serial_print(" | ");
    flag = (istat & 0x40)? "ATTACH" : UNSET;
    serial_print(flag);
    
    serial_print(" | ");
    flag = (istat & 0x20)? "RESUME" : UNSET;
    serial_print(flag);
    
    serial_print(" | ");
    flag = (istat & 0x10)? "SLEEP " : UNSET;
    serial_print(flag);
    
    serial_print(" | ");
    flag = (istat & 0x08)? "TOKDNE" : UNSET;
    serial_print(flag);
    
    serial_print(" | ");
    flag = (istat & 0x04)? "SOFTOK" : UNSET;
    serial_print(flag);
    
    serial_print(" | ");
    flag = (istat & 0x02)? "ERROR " : UNSET;
    serial_print(flag);
    
    serial_print(" | ");
    flag = (istat & 0x01)? "USBRST" : UNSET;
    serial_print(flag);
    serial_print(" ]");
#undef UNSET
}

void sput_stat(uint8_t stat) {
    serial_print("stat (0x");
    serial_phex(stat);
    serial_print(") [  ");
    serial_phex(USB_STAT_ENDP(stat));
    serial_print("  | ");
    serial_print(stat & USB_STAT_TX? " TX " : " RX ");
    serial_print(" | "); 
    serial_print(stat & USB_STAT_ODD? "ODD " : "EVEN");
    serial_print(" ]");
}

void sput_wRequestAndType(uint16_t wRequestAndType) {
    uint8_t request = wRequestAndType >> 8;
    uint8_t type    = wRequestAndType & 0xff; 
    const char *flag;
    switch (request) {
    case GET_STATUS:                flag = "GET_STATUS           "; break;
    case CLEAR_FEATURE:             flag = "CLEAR_FEATURE        "; break;
    case SET_FEATURE:               flag = "SET_FEATURE          "; break;
    case SET_ADDRESS:               flag = "SET_ADDRESS          "; break;
    case GET_DESCRIPTOR:            flag = "GET_DESCRIPTOR       "; break;
    case SET_DESCRIPTOR:            flag = "SET_DESCRIPTOR       "; break;
    case GET_CONFIGURATION:         flag = "GET_CONFIGURATION    "; break;
    case SET_CONFIGURATION:         flag = "SET_CONFIGURATION    "; break;
    case GET_INTERFACE:             flag = "GET_INTERFACE        "; break;
    case SET_INTERFACE:             flag = "SET_INTERFACE        "; break;
    case SYNC_FRAME:                flag = "SYNC_FRAME           "; break;
    case SET_SEL:                   flag = "SET_SEL              "; break;
    case SET_ISOCHRONOUS_DELAY:     flag = "SET_ISOCHRONOUS_DELAY"; break;
    default:                        flag = "UNKNOWN              "; break;
    }
    
    serial_print("wRequestAndType(0x");
    serial_phex16(wRequestAndType);
    serial_print("): [");
    serial_print(flag);
    serial_print(" [ ");
    serial_print(type & RT_IN? "IN " : "OUT");
    serial_print(" | ");
    serial_print(
        type & RT_CLASS? "CLASS   " : type & RT_VENDOR? "VENDOR  " : "STANDARD"
    );
    serial_print(" | ");
    switch (type & 0x1f) {
    case RT_DEVICE:     flag = "DEVICE   "; break;
    case RT_INTERFACE:  flag = "INTERFACE"; break;
    case RT_ENDPOINT:   flag = "ENDPOINT "; break;
    case RT_OTHER:      flag = "OTHER    "; break;
    default:            flag = "UNKNOWN  "; break;
    }
    serial_print(flag);
    serial_print(" ] ]");
}

void sput_pid(uint16_t pid) {
    const char *str = "unknown";
    switch (pid) 
    {
    case PID_OUT:   str = "PID_OUT";    break;
    case PID_IN:    str = "PID_IN";     break;
    case PID_SOF:   str = "PID_SOF";    break;
    case PID_SETUP: str = "PID_SETUP";  break;
    case PID_DATA0: str = "PID_DATA0";  break;
    case PID_DATA1: str = "PID_DATA1";  break;
    case PID_DATA2: str = "PID_DATA2";  break;
    case PID_MDATA: str = "PID_MDATA";  break;
    case PID_ACK:   str = "PID_ACK";    break;
    case PID_NAK:   str = "PID_NAK";    break;
    case PID_STALL: str = "PID_STALL";  break;
    case PID_NYET:  str = "PID_NYET";   break;
    }
    serial_print(str);
}

#else
void serial_printf(const char *fmt, ...) {}
void serial_xxd(const void *bytes, uint16_t length) {}
void sput_istat(uint8_t istat) {}
void sput_stat(uint8_t stat) {}
void sput_wRequestAndType(uint16_t wRequestAndType) {}
void sput_pid(uint16_t pid){}
#endif
