#include "usb_dev.h"
#include "usb_desc.h"
#include "usb_bdt.h"
#include "usb_names.h" /* struct usb_string_descriptor_struct */
#include "usb_msd.h"
#include "kinetis.h"
#include "serialize.h"

#include "core_pins.h"

/******************************************************************************/

// Offsets of bits in the USB_STAT register, see USB_STAT_TX & USB_STAT_ODD
#define USB_STAT_TX_(stat) ((stat & USB_STAT_TX) >> 3)
#define USB_STAT_ODD_(stat) ((stat & USB_STAT_ODD) >> 2) 

// Defined Request Codes for Control Transfers for USB 2.0 and 3.1
#define CONTROL_REQUEST_GET_STATUS              (0x00)
#define CONTROL_REQUEST_CLEAR_FEATURE           (0x01)
#define CONTROL_REQUEST_SET_FEATURE             (0x03)
#define CONTROL_REQUEST_SET_ADDRESS             (0x05)
#define CONTROL_REQUEST_GET_DESCRIPTOR          (0x06)
#define CONTROL_REQUEST_SET_DESCRIPTOR          (0x07)
#define CONTROL_REQUEST_GET_CONFIGURATION       (0x08)
#define CONTROL_REQUEST_SET_CONFIGURATION       (0x09)
#define CONTROL_REQUEST_GET_INTERFACE           (0x0a)
#define CONTROL_REQUEST_SET_INTERFACE           (0x0b)
#define CONTROL_REQUEST_SYNC_FRAME              (0x0c)
#define CONTROL_REQUEST_SET_SEL                 (0x30)
#define CONTROL_REQUEST_SET_ISOCHRONOUS_DELAY   (0x31)

// USB 2.0 and 3.1 requests for control transfers 
// USB Complete Fifth Edition: Page 131 Table 5-1
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

// masks for working on bmRequestType field
#define RT_DIRECTION    (0x80)
#define RT_TYPE         (0x60)
#define RT_RECIPIENT    (0x1f)

// build the 16 bit represenation of bRequest and bmRequestType
#define WREQUESTANDTYPE(bRequest, bmRequestType)                              \
    ((((uint16_t) (bRequest)) << 8) | (bmRequestType))

// USB feature codes
#define ENDPOINT_HALT   (0x00)

/******************************************************************************/

// handle a standard USB RESET from the host
static void reset(void);

// handle a USB SETUP control transfer on EP0
static void control_setup(setup_t *setup);

static void ep_nop_handler(bdt_t *bd);

// handle everything on Endpoint 0
static void ep0_handler(bdt_t *bd);

void usb_ep1_handler(bdt_t *)  __attribute__((weak, alias("ep_nop_handler")));
void usb_ep2_handler(bdt_t *)  __attribute__((weak, alias("ep_nop_handler")));
void usb_ep3_handler(bdt_t *)  __attribute__((weak, alias("ep_nop_handler")));
void usb_ep4_handler(bdt_t *)  __attribute__((weak, alias("ep_nop_handler")));
void usb_ep5_handler(bdt_t *)  __attribute__((weak, alias("ep_nop_handler")));
void usb_ep6_handler(bdt_t *)  __attribute__((weak, alias("ep_nop_handler")));
void usb_ep7_handler(bdt_t *)  __attribute__((weak, alias("ep_nop_handler")));
void usb_ep8_handler(bdt_t *)  __attribute__((weak, alias("ep_nop_handler")));
void usb_ep9_handler(bdt_t *)  __attribute__((weak, alias("ep_nop_handler")));
void usb_ep10_handler(bdt_t *) __attribute__((weak, alias("ep_nop_handler")));
void usb_ep11_handler(bdt_t *) __attribute__((weak, alias("ep_nop_handler")));
void usb_ep12_handler(bdt_t *) __attribute__((weak, alias("ep_nop_handler")));
void usb_ep13_handler(bdt_t *) __attribute__((weak, alias("ep_nop_handler")));
void usb_ep14_handler(bdt_t *) __attribute__((weak, alias("ep_nop_handler")));
void usb_ep15_handler(bdt_t *) __attribute__((weak, alias("ep_nop_handler")));

static void ep0_transmit(const void *data, uint32_t length);

/******************************************************************************/

/* current active configuration after a SET_CONFIGURATION control request */
volatile uint8_t usb_active_configuration = 0;

/*
 * Buffer Descriptor Table hold the ping pong buffers for each endpoint
 *
 * TX and RX both need two buffers, therefore if an endpoint is bidirectional 
 * 4 buffers are needed
 */
__attribute__((section(".usbdescriptortable"), used))
bdt_t bdt[(NUM_ENDPOINTS + 1) * 4];

/* buffers to hold incoming data HOST to DEVICE, one for each rx bdt entry */
static uint8_t ep0_rx[2][EP0_SIZE] __attribute__((aligned(4)));

/* flags to indicate which buffer to use, even/odd, data0/data1 */
static int ep0_odd_toggle = EVEN;
static int ep0_data_toggle = DATA0;

/* pointer to any data needed to be sent on endpoint 0 to the host */
static const void *ep0_tx_data = NULL;
static uint16_t ep0_tx_datalen = 0;

static void (*handlers[16])(bdt_t *bd) = {
    ep0_handler,
    usb_ep1_handler,
    usb_ep2_handler,
    usb_ep3_handler,
    usb_ep4_handler,
    usb_ep5_handler,
    usb_ep6_handler,
    usb_ep7_handler,
    usb_ep8_handler,
    usb_ep9_handler,
    usb_ep10_handler,
    usb_ep11_handler,
    usb_ep12_handler,
    usb_ep13_handler,
    usb_ep14_handler,
    usb_ep15_handler
};

/******************************************************************************/

void usb_init(void)
{
/* for logging we write to the hardware serial 1 */
#ifdef DEBUG
    #ifndef SERIAL_BAUD
        #warning SERIAL_BAUD undefined defaulting to 115200
        #define SERIAL_BAUD 115200
    #endif
	serial_begin(BAUD2DIV(SERIAL_BAUD));
    LOGINFO("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
#endif
    
	int i;
	usb_init_serialnumber();

	for (i = 0; i < ((NUM_ENDPOINTS + 1) * 4); i++) {
		bdt[i].desc = 0;
		bdt[i].addr = NULL;
	}

	// this basically follows the flowchart in the Kinetis
	// Quick Reference User Guide, Rev. 1, 03/2012, page 141

	// assume 48 MHz clock already running
	// SIM - enable clock
	SIM_SCGC4 |= SIM_SCGC4_USBOTG;
#ifdef HAS_KINETIS_MPU
	MPU_RGDAAC0 |= 0x03000000;
#endif

	// set desc table base addr
	USB0_BDTPAGE1 = ((uint32_t) bdt) >> 8;
	USB0_BDTPAGE2 = ((uint32_t) bdt) >> 16;
	USB0_BDTPAGE3 = ((uint32_t) bdt) >> 24;

	// clear all ISR flags
	USB0_ISTAT      = 0xff;
	USB0_ERRSTAT    = 0xff;
	USB0_OTGISTAT   = 0xff;

	// enable USB
	USB0_CTL = USB_CTL_USBENSOFEN;
	USB0_USBCTRL = 0;

	// enable reset interrupt
	USB0_INTEN = USB_INTEN_USBRSTEN;

	// enable interrupt in NVIC...
	NVIC_SET_PRIORITY(IRQ_USBOTG, 112);
	NVIC_ENABLE_IRQ(IRQ_USBOTG);

	// enable d+ pullup
	USB0_CONTROL = USB_CONTROL_DPPULLUPNONOTG;
}

// 41.5.9 Interrupt Status register (USBx_ISTAT)
// 41.5.13 Status register (USBx_STAT)
void usb_isr(void) {
    uint8_t status;
	uint8_t stat;
	uint8_t endpoint, tx, odd; // values stored in USB0_STAT
    
	restart:
	status = USB0_ISTAT;
    
	if (status & USB_ISTAT_SOFTOK) {
		USB0_ISTAT = USB_ISTAT_SOFTOK;
	}
	
	if (status & USB_ISTAT_TOKDNE) {
		stat = USB0_STAT;
        /*sput_stat(stat); sprint("\n");*/
		endpoint = USB_STAT_ENDP(stat);
		// find the bd and hand execution to the callback
        tx = USB_STAT_TX_(stat);
        odd = USB_STAT_ODD_(stat);
		handlers[endpoint](&bdt[BDT_INDEX(endpoint, tx, odd)]);
		USB0_ISTAT = USB_ISTAT_TOKDNE;
        goto restart; // TODO why?
	}
    
	if (status & USB_ISTAT_USBRST) {
        reset();
        return;
	}

	if (status & USB_ISTAT_STALL) {
		USB0_ENDPT0 = USB_ENDPT_EPRXEN | USB_ENDPT_EPTXEN | USB_ENDPT_EPHSHK;
		USB0_ISTAT = USB_ISTAT_STALL;
	}
	
	// TODO USB Error Register
	if (status & USB_ISTAT_ERROR) {
		uint8_t err = USB0_ERRSTAT;
        LOGCRITICAL("usb error status: 0x%x", (int) err);
		USB0_ERRSTAT = err;
		USB0_ISTAT = USB_ISTAT_ERROR;
	}

    // TODO USB Power Management
	if (status & USB_ISTAT_SLEEP) {
		USB0_ISTAT = USB_ISTAT_SLEEP;
	}
}

void usb_stall_endpoint(uint8_t ep) {
    LOGINFO("stalling endpoint %hhd", ep);
    (*(uint8_t *) (&USB0_ENDPT0 + (ep * 4))) |=  USB_ENDPT_EPSTALL;
}

/******************************************************************************/

void ep_nop_handler(bdt_t *bd) {
    (void) bd; /* mark as used to disable -Wunused-parameter warning */
    LOGERROR("by all rights we shouldn't be here");
}

void ep0_handler(bdt_t *bd) {
    static setup_t last_setup;
    
    const uint8_t* data = NULL;
    uint32_t size = 0;

    switch (BDT_PID(bd->desc)) 
    {
    case PID_SETUP:
		last_setup = *((setup_t *) bd->addr);
		
		/* we are now done with the buffer */
        bd->desc = BDT_DESC(EP0_SIZE, DATA1);

        /* clear any pending IN stuff */
        ep0_tx_data = NULL;
        ep0_tx_datalen = 0;
        bdt[BDT_INDEX(0, TX, EVEN)].desc = 0;
		bdt[BDT_INDEX(0, TX, ODD)].desc = 0;
        ep0_data_toggle = DATA1; /* first IN after SETUP is always DATA1 */

        /* run the setup */
        control_setup(&last_setup);
        break;
        
    case PID_IN:
        /* previous packet sent was recieved successfully */
        /* continue sending any pending transmit data */
        data = ep0_tx_data;
		if (data != NULL) {
			size = ep0_tx_datalen;
			if (size > EP0_SIZE) { size = EP0_SIZE; }
			ep0_transmit(data, size);
			data += size;
			ep0_tx_datalen -= size;
			ep0_tx_data = (ep0_tx_datalen > 0 || size == EP0_SIZE)? data : NULL;
		}

        if (last_setup.wRequestAndType == 
                WREQUESTANDTYPE(SET_ADDRESS, RT_OUT|RT_STANDARD|RT_DEVICE)) {
            last_setup.bRequest = 0; // TODO WHY?
            USB0_ADDR = last_setup.wValue;
        }
        break;
        
    case PID_OUT:
        // TODO ??
        // nothing to do here..just give the buffer back
        bd->desc = BDT_DESC(EP0_SIZE, DATA1);
        break;
        
    default:
        LOGERROR("unhandled pid 0x%hx", BDT_PID(bd->desc));
        break;
    }

    USB0_CTL = USB_CTL_USBENSOFEN;
}

void reset(void) {
    LOGINFO("USB RESET");
    // 41.5.14: reset all ping pong fields to 0
	USB0_CTL = USB_CTL_ODDRST;
	
	// intialize endpoint 0 buffers
    ep0_odd_toggle = EVEN;
	bdt[BDT_INDEX(0, RX, EVEN)].desc = BDT_DESC(EP0_SIZE, DATA0);
	bdt[BDT_INDEX(0, RX, EVEN)].addr = ep0_rx[0];
	bdt[BDT_INDEX(0, RX, ODD)].desc  = BDT_DESC(EP0_SIZE, DATA0);
	bdt[BDT_INDEX(0, RX, ODD)].addr  = ep0_rx[1];
	bdt[BDT_INDEX(0, TX, EVEN)].desc = 0;
	bdt[BDT_INDEX(0, TX, ODD)].desc  = 0;

	// activate endpoint 0 (41.5.23 Endpoint Control register (USBx_ENDPTn)) 
	// transmit, recieve, handshake
	USB0_ENDPT0 = USB_ENDPT_EPRXEN | USB_ENDPT_EPTXEN | USB_ENDPT_EPHSHK;
	
	// TODO setup USB0_EDNPT* control registers of all enpoints used

	// clear all ending interrupts
	USB0_ERRSTAT = 0xff;
	USB0_ISTAT = 0xff;

	// set the address to zero during enumeration: USB Specification
	USB0_ADDR = 0;

	// enable other interrupts
	USB0_ERREN = 0xff;
	USB0_INTEN = USB_INTEN_TOKDNEEN | USB_INTEN_SOFTOKEN | USB_INTEN_STALLEN |
		USB_INTEN_ERROREN |	USB_INTEN_USBRSTEN | USB_INTEN_SLEEPEN;

    // is this necessary?
    USB0_CTL = USB_CTL_USBENSOFEN;
}

void control_setup(setup_t *setup) {
    const usb_descriptor_list_t *entry;
    const uint8_t *data;
    uint8_t datalen;
    uint32_t size;
    uint8_t buffer[2];
    uint8_t i;
    
    
    data = NULL;
    datalen = 0;
    size = 0;
    
    switch (setup->wRequestAndType) 
    {
    // GET DESCRIPTOR //////////////////////////////////////////////////////////
    case WREQUESTANDTYPE(GET_DESCRIPTOR, RT_IN | RT_STANDARD | RT_DEVICE):
    //case WREQUESTANDTYPE(GET_DESCRIPTOR, RT_IN | RT_STANDARD | RT_INTERFACE):
        for (entry = usb_descriptor_list; entry->addr != NULL; entry++) {
            if (entry->wValue == setup->wValue && entry->wIndex == setup->wIndex) {
                data = entry->addr;
                
                // if string descriptor requested, get the length from the 
                // strings length field
                datalen = ((setup->wValue >> 8) == 3)?
                    ((struct usb_string_descriptor_struct *) data)->bLength :
                    entry->length;
                goto send;
            }
        }
        usb_stall_endpoint(0);
        return;
    
    // GET CONFIGURATION ///////////////////////////////////////////////////////
    case WREQUESTANDTYPE(GET_CONFIGURATION, RT_IN | RT_STANDARD | RT_DEVICE):
        buffer[0] = usb_active_configuration;
        data = buffer;
        datalen = 1;
        goto send;
    
    // SET CONFIGURATION ///////////////////////////////////////////////////////
    case WREQUESTANDTYPE(SET_CONFIGURATION, RT_OUT | RT_STANDARD | RT_DEVICE):
        // TODO wValue == 0????
        if (setup->wValue == 1) {
            usb_msd_init();
            usb_active_configuration = 1;
            goto send; // send a ZLP
        }
        usb_stall_endpoint(0);
        return;
        
    // GET STATUS (device) /////////////////////////////////////////////////////
    case WREQUESTANDTYPE(GET_STATUS, RT_IN | RT_STANDARD | RT_DEVICE):
        /* usb2.0 (bit 0: self/bus powered, bit 1: remote wakeup)
         * self powered, w/o remote wakeup is 0x00000 */
        buffer[0] = buffer[1] = 0; 
        data = buffer;
        datalen = 2;
        goto send;
        
    // GET STATUS (endpoint) ///////////////////////////////////////////////////
    case WREQUESTANDTYPE(GET_STATUS, RT_IN | RT_STANDARD | RT_ENDPOINT):
        /* bit 0 indicates if endpoint is in a halt condition */
        if (setup->wIndex > NUM_ENDPOINTS) {
            usb_stall_endpoint(0);
        }
        buffer[0] = buffer[1] = 0;
        // Formula in section 41.5.23
        // Address: 4007_2000h base + C0h offset + (4d × i), where i=0d to 15d
        if ((*(uint8_t*)(&USB0_ENDPT0+(setup->wIndex*4))) & USB_ENDPT_EPSTALL) {
            buffer[0] = 1; /* 0x0100 is 0x01 little endian*/
        }
        
        data = buffer;
        datalen = 2;
        goto send;
    
    // SET ADDRESS /////////////////////////////////////////////////////////////
    case WREQUESTANDTYPE(SET_ADDRESS, RT_OUT | RT_STANDARD | RT_DEVICE): 
        goto send; // handled in PID_IN, send a ZLP to acknowledge success
        
    // CLEAR FEATURE (endpoint) ////////////////////////////////////////////////
    // TODO Support Clear Feature and Set Feature for ENDPOINT_HALT
    case WREQUESTANDTYPE(CLEAR_FEATURE, RT_OUT | RT_STANDARD | RT_ENDPOINT):
        i = setup->wIndex & 0x7f; // TODO Why 0x7f and not 0x1f?
        if (i > NUM_ENDPOINTS || setup->wValue != ENDPOINT_HALT) {
            usb_stall_endpoint(0);
            return;
        }
        (*(uint8_t *) (&USB0_ENDPT0 + (i * 4))) &=  (~0x02); // toggle off
        goto send; // send ZLP
        
    // SET FEATURE (endpoint) //////////////////////////////////////////////////
    case WREQUESTANDTYPE(SET_FEATURE, RT_OUT | RT_STANDARD | RT_ENDPOINT):
        i = setup->wIndex & 0x7f; // TODO Why 0x7f and not 0x1f?
        if (i > NUM_ENDPOINTS || setup->wValue != ENDPOINT_HALT) {
            usb_stall_endpoint(0);
            return;
        }
        (*(uint8_t *) (&USB0_ENDPT0 + (i * 4))) |=  0x02; // toggle on
        goto send; // send ZLP
        
    // CLASS REQUESTS //////////////////////////////////////////////////////////
    
    case WREQUESTANDTYPE(GET_MAX_LUN, RT_IN | RT_CLASS | RT_INTERFACE):
        if (usb_active_configuration == 1) {
            buffer[0] = MSD_IMPL_MAX_LUN;
            ep0_transmit(buffer, 1);
        }
        break;
    
    case WREQUESTANDTYPE(BOMS_RESET, RT_OUT | RT_CLASS | RT_INTERFACE):
        if (usb_active_configuration == 1) {
            usb_msd_bulk_only_reset();
        }
        goto send; // send ZLP
    
    // UNSUPPORTED /////////////////////////////////////////////////////////////
    default:
        LOGERROR("recieved unsupported setup packet of type: 0x%04hx", 
            setup->wRequestAndType);
        usb_stall_endpoint(0);
        return;
    }
    
    send:
        // if datalen is greater than the expected size, truncate it
        if (datalen > setup->wLength) { datalen = setup->wLength; }
        
        // Chunk 1
        size = datalen;
        if (size > EP0_SIZE) { size = EP0_SIZE; }
        ep0_transmit(data, size);
        data += size;
        datalen -= size;
        // make sure to setup a ZLP if the amount of data requested is a
        // multiple of EP0_SIZE and the requested length is > then the 
        // amount of data to send back
        if (datalen == 0 && size < EP0_SIZE) { return; }
        
        // Chunk 2
        size = datalen;
        if (size > EP0_SIZE) { size = EP0_SIZE; }
        ep0_transmit(data, size);
        data += size;
        datalen -= size;
        if (datalen == 0 && size < EP0_SIZE) { return; } // ensure ZLP
        
        // if data remains store it
        ep0_tx_data     = data;
        ep0_tx_datalen  = datalen;
}

void ep0_transmit(const void *data, uint32_t length) {
    bdt[BDT_INDEX(0,TX,ep0_odd_toggle)].addr = (void *) data;
    bdt[BDT_INDEX(0,TX,ep0_odd_toggle)].desc = BDT_DESC(length,ep0_data_toggle);
    ep0_odd_toggle  ^= 1;
    ep0_data_toggle ^= 1;
}
