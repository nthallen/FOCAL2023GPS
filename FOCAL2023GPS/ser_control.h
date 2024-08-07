#ifndef SER_CONTROL_H_INCLUDED
#define SER_CONTROL_H_INCLUDED
#include <stdint.h>
#include "subbus.h"
#include "serial_num.h"

void SendMsg(const char *);			// Send String Back to Host via USB
void SendCode(int8_t code);
void SendCodeVal(int8_t, uint16_t);
void SendErrorMsg(const char *msg);
void poll_control(void);
extern subbus_driver_t sb_control;
#define CONTROL_BASE_ADDR 0x0B
#define CONTROL_HIGH_ADDR 0x0A

#ifdef CTRL_UART
#include "usart.h"
#define ctrl_init() uart_init()
#define ctrl_recv(buf,nbytes) uart_recv(buf,nbytes)
#define ctrl_send_char(c) uart_send_char(c)
#define ctrl_flush_input() uart_flush_input()
#define ctrl_flush_output() uart_flush_output()
#endif

#ifdef CTRL_USB_SER
#include "gps_usb.h"
#define ctrl_init() usb_ser_init()
#define ctrl_recv(buf,nbytes) usb_ser_recv(buf,nbytes)
#define ctrl_send_char(c) usb_ser_send_char(c)
#define ctrl_flush_input() usb_ser_flush_input()
#define ctrl_flush_output() usb_ser_flush_output()
#endif

#endif