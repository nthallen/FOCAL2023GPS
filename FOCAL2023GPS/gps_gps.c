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
static int usb_nw = 0;

static void gps_poll() {
  int nb = GPS_RX_BUF_SIZE - gps_nc - 1;
  if (nb > 0 && gps_nw == 0) {
    int nr = uart_recv(&gps_rx_buf[gps_nc], nb);
    if (nr>0) {
      gps_nc += nr;
    }
  }
  if (gps_nc > gps_nw) {
    int gps_to_write = gps_nw;
    if (gps_nc >= GPS_RX_BUF_SIZE-1) {
      gps_to_write = gps_nc;
    } else {
      for (int i = gps_nw; i < gps_nc; ++i) {
        if (gps_rx_buf[i] == '\n') {
          gps_to_write = i;
        }
      }
    }
    if (gps_to_write > gps_nw) {
      int nw = usb_ser_write(gps_rx_buf+gps_nw, gps_to_write-gps_nw);
      if (nw == gps_nc-gps_nw) {
        gps_nc = 0;
        gps_nw = 0;
      } else {
        gps_nw += nw;
        memmove(&gps_rx_buf[0], &gps_rx_buf[gps_nw], gps_nc-gps_nw);
        gps_nc -= gps_nw;
        gps_nw = 0;
      }
    }
  }

  nb = USB_RX_BUF_SIZE - usb_nc;
  if (nb > 0 && usb_nw == 0) {
    int nr = usb_ser_recv(&usb_rx_buf[usb_nc], nb);
    if (nr) {
      usb_nc += nr;
    }
  }
  if (usb_nc > usb_nw) {
    int usb_to_write = usb_nw;
    if (usb_nc >= USB_RX_BUF_SIZE-1) {
      usb_to_write = usb_nc;
    } else {
      for (int i = usb_nw; i < usb_nc; ++i) {
        if (usb_rx_buf[i] == '\n') {
          usb_to_write = i+1;
        }
      }
    }
    if (usb_to_write > usb_nw) {
      int nw = usb_ser_write(usb_rx_buf+usb_nw, usb_to_write-usb_nw);
      uart_write(usb_rx_buf+usb_nw, usb_to_write-usb_nw);
      if (nw == usb_nc-usb_nw) {
        usb_nc = 0;
        usb_nw = 0;
      } else {
        usb_nw += nw;
        memmove(&usb_rx_buf[0], &usb_rx_buf[usb_nw], usb_nc-usb_nw);
        usb_nc -= usb_nw;
        usb_nw = 0;
      }
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
