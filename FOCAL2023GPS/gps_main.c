#ifdef __cplusplus
extern "C" {
#endif

#include "gps_drv_init.h"
#include "gps_usb.h"
#include "ser_control.h"
#include "rtc_timer.h"

#ifdef __cplusplus
}
#endif

int main(void)
{
	/* Initializes MCU, drivers and middleware */
	system_init();

  if (subbus_add_driver(&sb_base)
   || subbus_add_driver(&sb_fail_sw)
   || subbus_add_driver(&sb_board_desc)
   || subbus_add_driver(&sb_control)
   || subbus_add_driver(&sb_usb)
   || subbus_add_driver(&sb_rtc)
  ) {
    while (true) ; // some driver is misconfigured.
  }
  subbus_reset(); // Resets all drivers
  while (1) {
    subbus_poll();
  }
}
