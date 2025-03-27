#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>

// Zephyr Headers
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "hid.h"
#include "imu/imu.h"

#include <zephyr/logging/log.h>

//This is my modification in main.c

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

static const struct gpio_dt_spec air_mouse_btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios);
static struct gpio_callback air_mouse_btn_cb_data;

void write_to_mouse_hid(uint8_t x, uint8_t y) {
  uint8_t message[4] = {0};
  message[1] = x;
  message[2] = y;

  int ret = hid_write(message);
  if (ret) {
    LOG_WRN("Could not write to USB %d, %d: %d", message[1], message[2], ret);
  }
}

void change_imu_status_button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
  imu_switch_status();
}

int main(void) {
  LOG_INF("Started IMU application");

  int ret;

  ret = hid_devices_init();
  if (ret) {
    LOG_ERR("Unable to start up HID device: %d", ret);
    return ret;
  }

  // Setting up GPIO to enable / disable on button click
  gpio_init_callback(&air_mouse_btn_cb_data, change_imu_status_button_pressed, BIT(air_mouse_btn.pin));
  gpio_add_callback(air_mouse_btn.port, &air_mouse_btn_cb_data);

  ret = imu_device_init(false, write_to_mouse_hid);
  if (ret) {
    LOG_ERR("Unable to start IMU: %d", ret);
    return ret;
  }

  while (1) {
    k_msleep(1);
  }
}
