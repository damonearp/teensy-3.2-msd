#ifndef _serialize_h_
#define _serialize_h_

#include <stdint.h>

#include "HardwareSerial.h"

#define LLCRITICAL  1
#define LLERROR     2
#define LLWARNING   4
#define LLINFO      8
#define LLDEBUG     16

#define LOG_LEVEL (LLCRITICAL | LLERROR | LLWARNING | LLINFO)


/* short hand for the serial functions provided by teensy core */
#define sprint      serial_print
#define sprinthex   serial_phex
#define sprinthex16 serial_phex16
#define sprinthex32 serial_phex32

/* short*/
#define sxxd(buffer, size)        serial_xxd((void*)(buffer),(size))

#define sprintln(msg)      do { sprint(msg);      sprint("\n"); } while (0)
#define sprinthexln(num)   do { sprinthex(num);   sprint("\n"); } while (0)
#define sprinthex16ln(num) do { sprinthex16(num); sprint("\n"); } while (0)
#define sprinthex32ln(num) do { sprinthex32(num); sprint("\n"); } while (0)

#define sprintptr(ptr) sprinthex32((uint32_t)ptr)
#define sprintptrln(ptr) sprinthex32ln((uint32_t)ptr)

#define sxxdptr(ptr,size) do {sprintptr(ptr);sprint(" ");sxxd(ptr,size);}while(0)

#define slog slogf
#define slogln(msg) slogf(msg "\n")
#define slogf(fmt, ...) serial_printf("%s(%d): " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define slogfln(fmt, ...) slogf(fmt "\n", ##__VA_ARGS__)

#if DEBUG && (LOG_LEVEL & LLCRITICAL)
#define LOGCRITICAL(fmt, ...) serial_printf("CRITICAL " fmt "\n", ##__VA_ARGS__)
#else
#define LOGCRITICAL(...)
#endif

#if DEBUG && (LOG_LEVEL & LLERROR)
#define LOGERROR(fmt, ...) serial_printf("ERROR " fmt "\n", ##__VA_ARGS__)
#else
#define LOGERROR(...)
#endif

#if DEBUG && (LOG_LEVEL & LLWARNING)
#define LOGWARN(fmt, ...)  serial_printf("WARN  " fmt "\n", ##__VA_ARGS__)
#else
#define LOGWARN(...)
#endif

#if DEBUG && (LOG_LEVEL & LLINFO)
#define LOGINFO(fmt, ...)  serial_printf("INFO  " fmt "\n", ##__VA_ARGS__)
#else
#define LOGINFO(...)
#endif

#if DEBUG && (LOG_LEVEL & LLDEBUG)
#define LOGDEBUG(fmt, ...) serial_printf("DEBUG %s[%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__) 
#else
#define LOGDEBUG(...)
#endif

/* serial printing of integers in base 10 and 16 */
#define sputint(i)  serial_printf("%d", (int32_t)  i)
#define sputuint(i) serial_printf("%u", (uint32_t) i)
#define sputhex   serial_phex
#define sputhex16 serial_phex16
#define sputhex32 serial_phex32

#ifdef __cplusplus
extern "C" {
#endif

void serial_printf(const char *fmt, ...);
// void serial_logf(const char *file, int line, const char *function, const char *fmt, ...);
void serial_xxd(const void *bytes, uint32_t length);

// use the serial.h to output a visual form of the USB0_ISTAT register
void sput_istat(uint8_t istat);
void sput_stat(uint8_t stat); // USB0_STAT register

// use serial.h to output a visual form of the wRequestAndType field
void sput_wRequestAndType(uint16_t wRequestAndType);

// usb bdt desc pid
void sput_pid(uint16_t pid); 

#ifdef __cplusplus
}
#endif

#endif
