#include <peripheral_clk_config.h>
#include <hpl_gclk_base.h>
#include <hpl_pm_base.h>
#include <hal_usart_async.h>
#include "gps_pins.h"
// #include "uDACS_driver_init.h"
#include "usart.h"


static struct usart_async_descriptor USART_0;
/*! Buffer for the receive ringbuffer */
static uint8_t USART_0_rx_buffer[USART_RX_BUFFER_SIZE];

/*! Buffer to accumulate output before sending */
static uint8_t USART_0_tx_buffer[USART_TX_BUFFER_SIZE];
 /*! The number of characters in the tx buffer */
static int nc_tx;
volatile int USART_0_tx_busy = 0;
static struct io_descriptor *USART_0_io;

#ifdef __cplusplus
extern "C" {
#endif


/**
 * \brief USART Clock initialization function
 *
 * Enables register interface and peripheral clock
 * Copied from driver_init.c: will need manual editing when that changes
 */
void USART_0_CLOCK_init() {
  hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM5_GCLK_ID_CORE, CONF_GCLK_SERCOM5_CORE_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));
  hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM5_GCLK_ID_SLOW, CONF_GCLK_SERCOM5_SLOW_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));
  hri_mclk_set_APBDMASK_SERCOM5_bit(MCLK);
}

/**
 * \brief USART pinmux initialization function
 *
 * Set each required pin to USART functionality
 * Copied from driver_init.c: will need manual editing when that changes
 */
void USART_0_PORT_init() {
  gpio_set_pin_function(PB16, PINMUX_PB16C_SERCOM5_PAD0);
  gpio_set_pin_function(RX, PINMUX_PB17C_SERCOM5_PAD1);
}


static void tx_cb_USART_0(const struct usart_async_descriptor *const io_descr) {
	/* Transfer completed */
	USART_0_tx_busy = 0;
}

/**
 * \brief Callback for received characters.
 * We do nothing here, but if we don't set it up, the low-level receive character
 * function won't be called either. This is of course undocumented behavior.
 */
static void rx_cb_USART_0(const struct usart_async_descriptor *const io_descr) {}

/**
 * \brief USART initialization function
 *
 * Enables USART peripheral, clocks and initializes USART driver
 */
void USART_0_init(void) {
	USART_0_PORT_init();
	USART_0_CLOCK_init();
	usart_async_init(&USART_0, SERCOM5, USART_0_rx_buffer,
    	USART_RX_BUFFER_SIZE, (void *)NULL);
	USART_0_PORT_init();
}

static void USART_0_write(const uint8_t *text, int count) {
	while (USART_0_tx_busy) {}
	USART_0_tx_busy = 1;
	io_write(USART_0_io, text, count);
}

void uart_init(void) {
	USART_0_init();
	usart_async_register_callback(&USART_0, USART_ASYNC_TXC_CB, tx_cb_USART_0);
	usart_async_register_callback(&USART_0, USART_ASYNC_RXC_CB, rx_cb_USART_0);
	usart_async_register_callback(&USART_0, USART_ASYNC_ERROR_CB, 0);
	usart_async_get_io_descriptor(&USART_0, &USART_0_io);
	usart_async_enable(&USART_0);
  nc_tx = 0;
}

int uart_recv(uint8_t *buf, int nbytes) {
	return io_read(USART_0_io, (uint8_t *)buf, nbytes);
}

void uart_flush_input(void) {
  usart_async_flush_rx_buffer(&USART_0);
}

void uart_send_char(uint8_t c) {
  if (nc_tx >= USART_TX_BUFFER_SIZE) {
    /* We can't be flushing, or nc_tx would be zero. Characters cannot be
       added to the buffer until _tx_busy is clear. */
    assert(USART_0_tx_busy == 0,__FILE__,__LINE__);
    uart_flush_output();
  }
  while (USART_0_tx_busy) {}
  assert(nc_tx < USART_TX_BUFFER_SIZE,__FILE__,__LINE__);
  USART_0_tx_buffer[nc_tx++] = c;
}

void uart_flush_output(void) {
  int nc = nc_tx;
  assert(USART_0_tx_busy == 0,__FILE__,__LINE__);
  nc_tx = 0;
  USART_0_write(USART_0_tx_buffer, nc);
}

/**
 * Sends String Back to Host via USB, appending a newline and
 * strobing the FTDI "Send Immediate" line. Every response
 * should end by calling this function.
 *
 * @param    String to be sent back to Host via USB
 * @return   None
 *
 */
void uart_write(const char *msg, int n) {
  while (n-- > 0) {
    uart_send_char(*msg++);
  }
  uart_flush_output();
}

#ifdef __cplusplus
};
#endif
