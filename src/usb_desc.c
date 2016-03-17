#if F_CPU >= 20000000

#define USB_DESC_LIST_DEFINE
#include <string.h> /* memcpy */
#include "usb_desc.h"
#include "usb_names.h"
#include "kinetis.h"
#include "avr_functions.h"

// **************************************************************
//   USB Device
// **************************************************************

#define LSB(n) ((n) & 255)
#define MSB(n) (((n) >> 8) & 255)

// These descriptors must NOT be "const", because the USB DMA
// has trouble accessing flash memory with enough bandwidth
// while the processor is executing from flash.


static dev_descriptor_t device_descriptor = {
    .bLength            = USB_DESCRIPTOR_DEVICE_LENGTH,
    .bDescriptorType    = USB_DESCRIPTOR_DEVICE_TYPE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0, // specified at interface 
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = EP0_SIZE,
    .idVendor           = 0x16c0,
    .idProduct          = 0x0484,
    .bcdDevice          = 0x0001,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

// the config descriptor, 1 interface with 2 endpoints
#define CONFIG_SIZE (                         \
    USB_DESCRIPTOR_CONFIGURATION_LENGTH     + \
    USB_DESCRIPTOR_INTERFACE_LENGTH         + \
    (2 * USB_DESCRIPTOR_ENDPOINT_LENGTH)      \
)

static cfg_descriptor_t config_descriptor = {
    .bLength                = USB_DESCRIPTOR_CONFIGURATION_LENGTH,
    .bDescriptorType        = USB_DESCRIPTOR_CONFIGURATION_TYPE,
    .wTotalLength           = CONFIG_SIZE,
    .bNumInterfaces         = 1,
    .bConfigurationValue    = 1,
    .iConfiguration         = 0,
    .bmAttributes           = 0x80,
    .bMaxPower              = 50,       // max milliamperes / 2
    .interfaces = {
        // Interface USB Mass Storage
        {
            .bLength            = USB_DESCRIPTOR_INTERFACE_LENGTH,
            .bDescriptorType    = USB_DESCRIPTOR_INTERFACE_TYPE,
            .bInterfaceNumber   = 0,
            .bAlternateSetting  = 0,
            .bNumEndpoints      = 2,
            .bInterfaceClass    = USB_DESCRIPTOR_CLASS_MSD,
            .bInterfaceSubClass = USB_DESCRIPTOR_INTERFACE_SUBCLASS_SCSI,
            .bInterfaceProtocol = USB_DESCRIPTOR_INTERFACE_PROTOCOL_BULKONLY,
            .iInterface         = 0,
            .endpoints = {
                // Endpoint 1 - OUT - RX (host to device)
                {
                    .bLength            = USB_DESCRIPTOR_ENDPOINT_LENGTH,
                    .bDescriptorType    = USB_DESCRIPTOR_ENDPOINT_TYPE,
                    .bEndpointAddress   = USB_DESCRIPTOR_ENDPOINT_ADDRESS_OUT(1),
                    .bmAttributes       = USB_DESCRIPTOR_ENDPOINT_ATTRIBUTE_BULKONLY,
                    .wMaxPacketSize     = EP1_SIZE,
                    .bInterval          = 0
                },
                // Endpoint 2 - IN - TX (device to host)
                {
                    .bLength            = USB_DESCRIPTOR_ENDPOINT_LENGTH,
                    .bDescriptorType    = USB_DESCRIPTOR_ENDPOINT_TYPE,
                    .bEndpointAddress   = USB_DESCRIPTOR_ENDPOINT_ADDRESS_IN(2),
                    .bmAttributes       = USB_DESCRIPTOR_ENDPOINT_ATTRIBUTE_BULKONLY,
                    .wMaxPacketSize     = EP2_SIZE,
                    .bInterval          = 0
                }
            }
        }
    }
};


// **************************************************************
//   String Descriptors
// **************************************************************

// The descriptors above can provide human readable strings,
// referenced by index numbers.  These descriptors are the
// actual string data

/* defined in usb_names.h
struct usb_string_descriptor_struct {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint16_t wString[];
};
*/

extern struct usb_string_descriptor_struct usb_string_manufacturer_name
        __attribute__ ((weak, alias("usb_string_manufacturer_name_default")));
extern struct usb_string_descriptor_struct usb_string_product_name
        __attribute__ ((weak, alias("usb_string_product_name_default")));
extern struct usb_string_descriptor_struct usb_string_serial_number
        __attribute__ ((weak, alias("usb_string_serial_number_default")));

struct usb_string_descriptor_struct string0 = {
        4,
        3,
        {0x0409}
};

struct usb_string_descriptor_struct usb_string_manufacturer_name_default = {
        2 + MANUFACTURER_NAME_LEN * 2,
        3,
        MANUFACTURER_NAME
};
struct usb_string_descriptor_struct usb_string_product_name_default = {
	2 + PRODUCT_NAME_LEN * 2,
        3,
        PRODUCT_NAME
};
struct usb_string_descriptor_struct usb_string_serial_number_default = {
        12,
        3,
        {0,0,0,0,0,0,0,0,0,0}
};

void usb_init_serialnumber(void)
{
	char buf[11];
	uint32_t i, num;

	__disable_irq();
	FTFL_FSTAT = FTFL_FSTAT_RDCOLERR | FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL;
	FTFL_FCCOB0 = 0x41;
	FTFL_FCCOB1 = 15;
	FTFL_FSTAT = FTFL_FSTAT_CCIF;
	while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) ; // wait
	num = *(uint32_t *)&FTFL_FCCOB7;
	__enable_irq();
	// add extra zero to work around OS-X CDC-ACM driver bug
	if (num < 10000000) num = num * 10;
	ultoa(num, buf, 10);
	for (i=0; i<10; i++) {
		char c = buf[i];
		if (!c) break;
		usb_string_serial_number_default.wString[i] = c;
	}
	usb_string_serial_number_default.bLength = i * 2 + 2;
}

// **************************************************************
//   Descriptors List
// **************************************************************

// This table provides access to all the descriptor data above.

const usb_descriptor_list_t usb_descriptor_list[] = {
	//wValue, wIndex, address,          length
	{0x0100, 0x0000, (void *) &device_descriptor, sizeof(device_descriptor)},
	{0x0200, 0x0000, (void *) &config_descriptor, CONFIG_SIZE},
        {0x0300, 0x0000, (const uint8_t *)&string0, 0},
        {0x0301, 0x0409, (const uint8_t *)&usb_string_manufacturer_name, 0},
        {0x0302, 0x0409, (const uint8_t *)&usb_string_product_name, 0},
        {0x0303, 0x0409, (const uint8_t *)&usb_string_serial_number, 0},
	{0, 0, NULL, 0}
};

#endif // F_CPU >= 20 MHz
