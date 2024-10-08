/*
 * This file is based on code originally generated from Atmel START as usb_start.c
 * Whenever the Atmel START project is updated, changes to usb_start.c must be
 * reviewed and copied here as appropriate.
 */
#include <peripheral_clk_config.h>
#include <utils.h>
#include <hal_init.h>

#include "gps_usb.h"
#include "usart.h"

// USB Driver
void USB_CTRL_PORT_init(void)
{
	gpio_set_pin_direction(PA24, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PA24, false);
	gpio_set_pin_pull_mode(PA24, GPIO_PULL_OFF);
	gpio_set_pin_function(PA24, PINMUX_PA24H_USB_DM);

	gpio_set_pin_direction(PA25, GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PA25, false);
	gpio_set_pin_pull_mode(PA25, GPIO_PULL_OFF);
	gpio_set_pin_function(PA25, PINMUX_PA25H_USB_DP);
}

/* The USB module requires a GCLK_USB of 48 MHz ~ 0.25% clock
 * for low speed and full speed operation. */
#if (CONF_GCLK_USB_FREQUENCY > (48000000 + 48000000 / 400)) || (CONF_GCLK_USB_FREQUENCY < (48000000 - 48000000 / 400))
#warning USB clock should be 48MHz ~ 0.25% clock, check your configuration!
#endif

void USB_CTRL_CLOCK_init(void)
{

	hri_gclk_write_PCHCTRL_reg(GCLK, USB_GCLK_ID, CONF_GCLK_USB_SRC | GCLK_PCHCTRL_CHEN);
	hri_mclk_set_AHBMASK_USB_bit(MCLK);
	hri_mclk_set_APBBMASK_USB_bit(MCLK);
}

void USB_CTRL_init(void)
{
	USB_CTRL_CLOCK_init();
	usb_d_init();
	USB_CTRL_PORT_init();
}
// End of USB Driver

volatile bool pending_read = false;
volatile bool pending_write = false;
volatile bool cdc_connected = false;
volatile bool usb_cdc_state_changed = false;
static uint8_t input_buffer[CDC_INPUT_BUFFER_SIZE];
static uint32_t input_length = 0;
static uint8_t output_buffer[CDC_OUTPUT_BUFFER_SIZE];
static uint16_t obuf_nc; // total number of bytes in output_buffer
static uint16_t obuf_written; // number of bytes that have been written
static uint16_t saw_write_collision;
static void cdc_flush_output();
// static uint16_t obuf_pending; // number of bytes in current write request

#if CONF_USBD_HS_SP
static uint8_t single_desc_bytes[] = {
    /* Device descriptors and Configuration descriptors list. */
    CDCD_ACM_HS_DESCES_LS_FS};
static uint8_t single_desc_bytes_hs[] = {
    /* Device descriptors and Configuration descriptors list. */
    CDCD_ACM_HS_DESCES_HS};
#define USBD_CDC_BUFFER_SIZE CONF_USB_CDCD_ACM_DATA_BULKIN_MAXPKSZ_HS
#else
static uint8_t single_desc_bytes[] = {
    /* Device descriptors and Configuration descriptors list. */
    CDCD_ACM_DESCES_LS_FS};
#define USBD_CDC_BUFFER_SIZE CONF_USB_CDCD_ACM_DATA_BULKIN_MAXPKSZ
#endif

static struct usbd_descriptors single_desc[]
    = {{single_desc_bytes, single_desc_bytes + sizeof(single_desc_bytes)}
#if CONF_USBD_HS_SP
       ,
       {single_desc_bytes_hs, single_desc_bytes_hs + sizeof(single_desc_bytes_hs)}
#endif
};

/** Ctrl endpoint buffer */
static uint8_t ctrl_buffer[64];

/** Buffers to receive communication bytes. */
static COMPILER_ALIGNED(4) uint8_t usbd_cdc_buffer[USBD_CDC_BUFFER_SIZE];

static uint32_t cdc_read_start() {
  pending_read = true;
  return cdcdf_acm_read(usbd_cdc_buffer, USBD_CDC_BUFFER_SIZE);
}

/**
 * @brief Callback invoked when bulk OUT data (from host to device) received
 * @return true on error
 */
static bool cdc_read_finished(const uint8_t ep, const enum usb_xfer_code rc, const uint32_t count) {
  int i = 0;
  bool rv = false;
  if (rc == USB_XFER_DONE) {
    pending_read = false;
    CRITICAL_SECTION_ENTER()
    while (input_length < CDC_INPUT_BUFFER_SIZE && i < count) {
      input_buffer[input_length++] = usbd_cdc_buffer[i++];
    }
    memset(usbd_cdc_buffer, 0, USBD_CDC_BUFFER_SIZE);
    CRITICAL_SECTION_LEAVE()
    if (i < count) {
      rv = true; // Not enough space
    }
    cdc_read_start();
  }
  return rv;
}

static int32_t cdc_write(const uint8_t *const buf, const uint16_t length) {
  bool flush = false;
  uint16_t bytes_written = length;
  if (obuf_nc) {
    ++saw_write_collision;
  }
  CRITICAL_SECTION_ENTER()
  if (obuf_nc+bytes_written > CDC_OUTPUT_BUFFER_SIZE) {
    bytes_written = CDC_OUTPUT_BUFFER_SIZE - obuf_nc;
    flush = true;
  }
  for (int i = 0; i < bytes_written && cdc_connected; ++i) {
    char p = buf[i];
    output_buffer[obuf_nc++] = p;
    if (p == '\n') flush = true;
  }
  CRITICAL_SECTION_LEAVE()
  if (flush) {
    cdc_flush_output();
  }
  return bytes_written;
}

static void cdc_flush_output() {
  int new_write = 0;
  int obuf_pending = 0;
  CRITICAL_SECTION_ENTER()
  // obuf_written can change asynchronously, along with obuf_nc, but
  // only if (pending_write).
  // On the other hand, we call this function
  // from cdc_write_finished(), which is called from in interrupt handler,
  // but again, only if pending_write is true. In other words, if we
  // get into this critical section and !pending_write is true, then
  // even though we could be interrupted at the LEAVE(), that won't
  // happen.
  if (!pending_write && obuf_nc > obuf_written) {
    pending_write = true;
    new_write = 1;
    obuf_pending = obuf_nc - obuf_written;
  }
  CRITICAL_SECTION_LEAVE()
  if (new_write) {
    cdcdf_acm_write(output_buffer+obuf_written, obuf_pending);
  }
}

/**
 * \brief Callback invoked when bulk IN data (from device to host) has been transmitted
 */
static bool cdc_write_finished(const uint8_t ep, const enum usb_xfer_code rc,
                               const uint32_t count) {
  int still_writing = 0;
  CRITICAL_SECTION_ENTER()
  obuf_written += count;
  obuf_nc -= obuf_written;
  memmove(&output_buffer[0], &output_buffer[obuf_written], obuf_nc);
  obuf_written = 0;
  if (obuf_nc) still_writing = 1;
  pending_write = false;
  CRITICAL_SECTION_LEAVE()
  if (still_writing) {
    cdc_flush_output();
  }
	/* No error. */
	return false;
}

/**
 * \brief Callback invoked when Line State Change
 */
static bool usb_device_state_changed_handler(usb_cdc_control_signal_t state) {
  if (state.rs232.DTR) {
    cdc_connected = true;
  } else {
    cdc_connected = false;
  }
	return false;
}

/**
 * \brief CDC ACM Init
 */
void cdc_device_acm_init(void) {
	/* usb stack init */
	usbdc_init(ctrl_buffer);

	/* usbdc_register_funcion inside */
	cdcdf_acm_init();

	usbdc_start(single_desc);
	usbdc_attach();
}


typedef enum {USB_Init, USB_Enabled, USB_Run} sb_usb_state_t;
static sb_usb_state_t sb_usb_state = USB_Init;

static void sb_usb_reset() {
	USB_CTRL_init();
	cdc_device_acm_init();
}

static void sb_usb_poll() {
  switch (sb_usb_state) {
    case USB_Init:
      if (cdcdf_acm_is_enabled()) {
      	cdcdf_acm_register_callback(CDCDF_ACM_CB_STATE_C, (FUNC_PTR)usb_device_state_changed_handler);
        sb_usb_state = USB_Enabled;
      }
      break;
    case USB_Enabled:
      if (cdc_connected) {
  	    /* Callbacks must be registered after endpoint allocation */
  	    cdcdf_acm_register_callback(CDCDF_ACM_CB_READ, (FUNC_PTR)cdc_read_finished);
  	    cdcdf_acm_register_callback(CDCDF_ACM_CB_WRITE, (FUNC_PTR)cdc_write_finished);
        cdc_read_start();
        sb_usb_state = USB_Run;
      }
      break;
    case USB_Run:
      if (!cdc_connected) {
        sb_usb_state = USB_Enabled;
      }
      break;
    default:
      sb_usb_state = USB_Init;
      break;
  }

}

/** Buffer for the receive ringbuffer */
// static uint8_t sb_acm_rx_buffer[USART_CTRL_RX_BUFFER_SIZE];

/** Buffer to accumulate output before sending */
// static uint8_t sb_acm_tx_buffer[USART_CTRL_TX_BUFFER_SIZE];
/** The number of characters in the tx buffer */
// static int nc_tx;
volatile int sb_acm_tx_busy = 0;

/**
 * Initialization of the usb hardware is handled by the usb
 * device driver.
 */
void usb_ser_init(void) {
  // nc_tx = 0;
  input_length = 0;
  obuf_nc = 0;
  // obuf_pending = 0;
  obuf_written = 0;
}

int usb_ser_recv(uint8_t *buf, int nbytes) {
  if (nbytes > 0) {
    CRITICAL_SECTION_ENTER()
    if (nbytes > input_length) {
      nbytes = input_length;
    }
    for (int i = 0; i < nbytes; ++i) {
      buf[i] = input_buffer[i];
    }
    if (input_length > nbytes) {
      input_length -= nbytes;
      memmove(&input_buffer[0], &input_buffer[nbytes], input_length);
    } else {
      input_length = 0;
    }
    CRITICAL_SECTION_LEAVE()
  }
  if (!pending_read) {
    cdc_read_start();
  }
  return nbytes;
}

void usb_ser_flush_input(void) {
  CRITICAL_SECTION_ENTER()
  input_length = 0;
  CRITICAL_SECTION_LEAVE()
}

int overflow = 0;

/**
 * Sends String Back to Host via USB, appending a newline and
 * strobing the FTDI "Send Immediate" line. Every response
 * should end by calling this function.
 *
 * @param    String to be sent back to Host via USB
 * @return   None
 *
 */
int usb_ser_write(const uint8_t *msg, int n) {
  int nw = cdc_write(msg, n);
  if (nw < n) {
    overflow += n-nw;
  }
  return nw;
}

void usb_ser_send_char(uint8_t c) {
  cdc_write(&c, 1);
}

void usb_ser_flush_output(void) {
  cdc_flush_output();
}

subbus_driver_t sb_usb = {
  SUBBUS_USB_BASE_ADDR, SUBBUS_USB_HIGH_ADDR,
  0, // No cache
  sb_usb_reset,
  sb_usb_poll,
   0, // No dynamic actions
  false, // Not initialized
  0   // Next
};
