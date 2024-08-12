/* gps_gps.c */
#include <stdint.h>
#include <stdio.h>
#include "subbus.h"
#include "gps_gps.h"
#include "usart.h"
#include "gps_usb.h"
#include "rtc_timer.h"

static void gps_reset() {
  uart_init();
  usb_ser_init();
}

static uint8_t gps_rx_buf[GPS_RX_BUF_SIZE];
static int gps_nc = 0;
static int gps_nw = 0;
static uint8_t usb_rx_buf[USB_RX_BUF_SIZE];
static int usb_nc = 0;
#ifdef GPS_TEST_OUTPUT
static uint32_t last_report = 0, next_report = 0;
#endif

static void gps_poll() {
  int nb = GPS_RX_BUF_SIZE - gps_nc - 1;
#ifndef GPS_TEST_OUTPUT
  if (nb > 0 && gps_nw == 0) {
    int nr = uart_recv(&gps_rx_buf[gps_nc], nb);
    if (nr) {
      gps_nc += nr;
    }
  }
#else
  // This is a hack to produce test output for testing RS422/485
  if (nb > 0 && gps_nw == 0 && (last_report == 0 ||
      rtc_current_count >= next_report /* (last_report+100000 RTC_COUNTS_PER_SECOND*/))
  {
     last_report = rtc_current_count;
     next_report += RTC_COUNTS_PER_SECOND;
     gps_nc = snprintf((char*)gps_rx_buf, GPS_RX_BUF_SIZE, "%lu,%lu,%lu\r\n", rtc_current_count, rtc_current_count, rtc_current_count);
     gps_nw = 0;
  }
#endif
  if (gps_nc > gps_nw) {
    int nw = usb_ser_write((char*)gps_rx_buf+gps_nw, gps_nc-gps_nw);
    uart_write((char *)(gps_rx_buf+gps_nw), gps_nc-gps_nw);
    if (nw == gps_nc-gps_nw) {
      gps_nc = 0;
      gps_nw = 0;
    } else {
      gps_nw += nw;
    }
  }
}

subbus_driver_t sb_gps = {
  SUBBUS_GPS_BASE_ADDR, SUBBUS_GPS_HIGH_ADDR, // address range
  0,
  gps_reset,
  gps_poll,
  0,
  false
};
