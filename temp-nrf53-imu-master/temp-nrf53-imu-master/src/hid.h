#ifndef _HID_H
#define _HID_H

#include <string.h>
#include <zephyr/device.h>

int hid_write(uint8_t message[4]);
int hid_devices_init(void);

#endif
