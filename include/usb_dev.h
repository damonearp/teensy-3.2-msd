
#ifndef usb_dev_h_
#define usb_dev_h_

#include "kinetis.h"

// Defined PID Values for Token, Data and Handshake phases
// USB Complete Fifth Edition: Page 44 Table 2-3 
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

#define RX   0
#define TX   1
#define ODD  1
#define EVEN 0
#define DATA0 0
#define DATA1 1

#define USB_DESC_LIST_DEFINE
#include "usb_desc.h"


typedef union {
 struct {
  union {
   struct {
	uint8_t bmRequestType;
	uint8_t bRequest;
   };
	uint16_t wRequestAndType;
  };
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
 };
 struct {
	uint32_t word1;
	uint32_t word2;
 };
} setup_t;


#if F_CPU >= 20000000 && !defined(USB_DISABLED)
//#include "usb_mem.h"

void usb_init(void);
void usb_init_serialnumber(void);
void usb_isr(void);

void usb_stall_endpoint(uint8_t ep);

/* What configuration has the host selected as the active one */
extern volatile uint8_t usb_active_configuration;

#else // F_CPU < 20000000

void usb_init(void);

#endif // F_CPU

#endif
