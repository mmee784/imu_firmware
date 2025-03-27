#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usb_device.h>

#include <zephyr/usb/class/usb_hid.h>

static const uint8_t mouse_report_desc[] = HID_MOUSE_REPORT_DESC(2);
const struct device *hid_mouse;

int hid_write(uint8_t message[4]) { return hid_int_ep_write(hid_mouse, message, 4, NULL); }

int hid_devices_init(void) {
  hid_mouse = device_get_binding("HID_0");
  if (hid_mouse == NULL) {
    return 1;
  }

  usb_hid_register_device(hid_mouse, mouse_report_desc, sizeof(mouse_report_desc), NULL);

  if (usb_hid_init(hid_mouse)) {
    return 1;
  }

  if (usb_enable(NULL)) {
    return 1;
  }

  return 0;
}
