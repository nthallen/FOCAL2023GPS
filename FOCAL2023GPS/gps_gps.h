#ifndef GPS_GPS_H_INCLUDED
#define GPS_GPS_H_INCLUDED

subbus_driver_t sb_gps;
#define SUBBUS_GPS_BASE_ADDR 0xC
#define SUBBUS_GPS_HIGH_ADDR (SUBBUS_USB_BASE_ADDR-1)

#define GPS_RX_BUF_SIZE 1024
#define USB_RX_BUF_SIZE 1024

#define GPS_TEST_OUTPUT

#endif
