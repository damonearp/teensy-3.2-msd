#ifndef _usb_desc_h_
#define _usb_desc_h_

#include <stdint.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
// Implementation details
#define MANUFACTURER_NAME       {'T','e','e','n','s','y','d','u','i','n','o'}
#define MANUFACTURER_NAME_LEN   11
#define PRODUCT_NAME            {'U','S','B',' ','M','S','D'}
#define PRODUCT_NAME_LEN        7
#define EP0_SIZE                64
#define EP1_SIZE                64
#define EP2_SIZE                64
#define NUM_ENDPOINTS           2 // ignoring endpoint 0 which has to be there
////////////////////////////////////////////////////////////////////////////////

// Device Classes specified in the Device/Interface descriptors
#define USB_DESCRIPTOR_CLASS_CDC_DATA               (0x0a)
#define USB_DESCRIPTOR_CLASS_MSD                    (0x08)

// 
#define USB_DESCRIPTOR_DEVICE_LENGTH                (0x12)
#define USB_DESCRIPTOR_DEVICE_TYPE                  (0x01)

#define USB_DESCRIPTOR_CONFIGURATION_LENGTH         (0x09)
#define USB_DESCRIPTOR_CONFIGURATION_TYPE           (0X02)

#define USB_DESCRIPTOR_INTERFACE_LENGTH             (0x09)
#define USB_DESCRIPTOR_INTERFACE_TYPE               (0x04)
#define USB_DESCRIPTOR_INTERFACE_SUBCLASS_SCSI      (0x06)
#define USB_DESCRIPTOR_INTERFACE_PROTOCOL_BULKONLY  (0x50)

#define USB_DESCRIPTOR_ENDPOINT_LENGTH              (0x07)
#define USB_DESCRIPTOR_ENDPOINT_TYPE                (0x05)
#define USB_DESCRIPTOR_ENDPOINT_ATTRIBUTE_BULKONLY  (0x02)

#define USB_DESCRIPTOR_ENDPOINT_ADDRESS_OUT(_num)   (_num)
#define USB_DESCRIPTOR_ENDPOINT_ADDRESS_IN(_num)    ((_num) | 0x80)

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute__((packed)) dev_descriptor_t;
 
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed)) ep_descriptor_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
    ep_descriptor_t endpoints[2];
} __attribute__((packed)) int_descriptor_t;
 
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
    int_descriptor_t interfaces[];
} __attribute__((packed)) cfg_descriptor_t;
 
typedef struct {
    uint16_t wValue;
    uint16_t wIndex;
    const void* addr;
    uint8_t length;
} descriptor_entry_t;


#ifdef USB_DESC_LIST_DEFINE
#if defined(NUM_ENDPOINTS) && NUM_ENDPOINTS > 0

typedef struct {
	uint16_t	wValue;
	uint16_t	wIndex;
	const uint8_t *addr;
	uint16_t	length;
} usb_descriptor_list_t;

extern const usb_descriptor_list_t usb_descriptor_list[];
#endif // NUM_ENDPOINTS
#endif // USB_DESC_LIST_DEFINE

#endif
