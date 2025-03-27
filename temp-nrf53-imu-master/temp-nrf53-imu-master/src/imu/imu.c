#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "inv_imu_driver.h"
#include "Message.h"
#include "ErrorHelper.h"
#include "inv_imu_extfunc.h"
#include "inv_imu_defs.h"
#include "invn_algo_aml.h"

#include "imu.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(imu, LOG_LEVEL_DBG);

static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(DT_NODELABEL(mysensor));
static const struct gpio_dt_spec imu_int = GPIO_DT_SPEC_GET(DT_NODELABEL(imu_int_2), gpios);
static struct gpio_callback imu_int_cb_data;

static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios, {0});
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios);

volatile bool data_available = false;
static bool imu_enabled = true;
static bool imu_changed_status = false;

int count=0;

imu_on_data_received callback;

static struct inv_imu_device icm_driver = {
    .fifo_is_used = 0, // Initialize to 0 if FIFO is not used initially
};

K_THREAD_STACK_DEFINE(imu_stack_area, CONFIG_IMU_THREAD_STACK_SIZE);
struct k_thread imu_thread_data;

void inv_imu_sleep_us(uint32_t us) { k_usleep(us); }
uint64_t inv_imu_get_time_us(void) { return k_uptime_get() * 1000; }

static void apply_mounting_matrix(const int32_t matrix[9], int16_t raw[3]) {
  unsigned i;
  int16_t out_raw[3];

  for (i = 0; i < 3; i++) {
    out_raw[i] = ((int16_t)matrix[3 * i + 0] * raw[0]);
    out_raw[i] += ((int16_t)matrix[3 * i + 1] * raw[1]);
    out_raw[i] += ((int16_t)matrix[3 * i + 2] * raw[2]);
  }

  raw[0] = out_raw[0];
  raw[1] = out_raw[1];
  raw[2] = out_raw[2];
}

int inv_io_hal_read_reg(struct inv_imu_serif *serif, uint8_t reg, uint8_t *rbuffer, uint32_t rlen) {
  const struct i2c_dt_spec *i2c = (const struct i2c_dt_spec *)serif->context;

  int ret;
  for (int i = 0; i < 2; i++) {
    ret = i2c_write_read_dt(i2c, &reg, 1, rbuffer, rlen);
    if (ret == 0)
      break;
  }

  return ret;
}

int inv_io_hal_write_reg(struct inv_imu_serif *serif, uint8_t reg, const uint8_t *wbuffer, uint32_t wlen) {
  const struct i2c_dt_spec *i2c = (const struct i2c_dt_spec *)serif->context;

  uint8_t write_buf[1 + wlen];
  write_buf[0] = reg;
  memcpy(&write_buf[1], wbuffer, wlen);

  return i2c_write_dt(i2c, write_buf, sizeof(write_buf));
}

void on_data_present(const struct device *dev, struct gpio_callback *cb, uint32_t pins) { data_available = true; }

void handle_movement_event(inv_imu_sensor_event_t *event) {
  InvnAlgoAMLInput aml_input;
  InvnAlgoAMLOutput aml_output;

  memset(&aml_input, 0, sizeof(aml_input));
  memset(&aml_output, 0, sizeof(aml_output));

  // Skipping processing if FIFO doesn't contain all required data 
  if ( !(event->sensor_mask & (1 << INV_SENSOR_ACCEL)) || !(event->sensor_mask & (1 << INV_SENSOR_GYRO)))
  {
  return;
  }

  aml_input.racc_data[0] = event->accel[0];
  aml_input.racc_data[1] = event->accel[1];
  aml_input.racc_data[2] = event->accel[2];
  aml_input.rgyr_data[0] = event->gyro[0];
  aml_input.rgyr_data[1] = event->gyro[1];
  aml_input.rgyr_data[2] = event->gyro[2];
  aml_input.click_button = 0;

  apply_mounting_matrix(accel_mounting_matrix, aml_input.racc_data);
  apply_mounting_matrix(gyro_mounting_matrix, aml_input.rgyr_data);

  invn_algo_aml_process(&aml_input, &aml_output);

/*  if (count<21) //added. ignore first 5 data points? ~ 100ms
  {
    count++;
    return; //don't execute callback
  }
*/
  if (aml_output.status & INVN_ALGO_AML_STATUS_DELTA_COMPUTED) {
    callback(aml_output.delta[0], aml_output.delta[1]);
  }
}

void imu_disable() {
  inv_imu_disable_accel(&icm_driver);
  k_msleep(5);

  inv_imu_disable_gyro(&icm_driver);
  k_msleep(5);
//  count=0;
}

void imu_enable() {
  inv_imu_enable_accel_low_noise_mode(&icm_driver);
  k_msleep(5);

  inv_imu_enable_gyro_low_noise_mode(&icm_driver);
  k_msleep(5);
}

void imu_switch_status() {
  if (imu_enabled) {
    LOG_INF("IMU is getting disabled");
    imu_enabled = false;
  } else {
    LOG_INF("IMU is getting enabled");
    imu_enabled = true;
  }

  imu_changed_status = true;
}

static void imu_thread_worker(void *arg1, void *arg2, void *arg3) {
  while (1) {
    if (imu_changed_status) {
      if (imu_enabled)
        imu_enable();
      else
        imu_disable();

      imu_changed_status = false;
      inv_imu_reset_fifo(&icm_driver);
    }

    if (imu_enabled) {
      if (data_available) {
        inv_imu_get_data_from_fifo(&icm_driver);
        data_available = false;
      }
    }

    k_msleep(1);
  }
}

int imu_device_init(bool enabled, imu_on_data_received external_callback) {
  int ret;

  callback = external_callback;
  imu_enabled = enabled;

  struct inv_imu_serif icm_serif;
  icm_serif.read_reg = inv_io_hal_read_reg;
  icm_serif.write_reg = inv_io_hal_write_reg;
  icm_serif.max_read = 1024 * 32;
  icm_serif.max_write = 1024 * 32;
  icm_serif.serif_type = INV_SERIF_TYPE_I2C;

  uint8_t who_am_i;
  icm_serif.context = (void *)&dev_i2c;

  ret = inv_imu_init(&icm_driver, &icm_serif, handle_movement_event);
  if (ret != INV_ERROR_SUCCESS) {
    LOG_WRN("Failed to initialize IMU: %d", ret);
    return ret;
  }

  ret = inv_imu_get_who_am_i(&icm_driver, &who_am_i);
  if (ret != INV_ERROR_SUCCESS) {
    LOG_WRN("Failed to read WHO_AM_I register: %d", ret);
    return ret;
  }

  if (who_am_i != ICM42670S_WHOAMI) {
    LOG_WRN("Unexpected WHO_AM_I value: 0x%x, expected: 0x%x", who_am_i, ICM42670S_WHOAMI);
    return -1;
  }

  InvnAlgoAMLConfig config;
  memset(&config, 0, sizeof(config));

  config.acc_fsr = 16;
  config.gyr_fsr = 2000;

  config.delta_gain[0] = INVN_ALGO_AML_DELTA_GAIN_DEFAULT;
  config.delta_gain[1] = INVN_ALGO_AML_DELTA_GAIN_DEFAULT;
  config.gestures_auto_reset = 0;

  ret = invn_algo_aml_init(&icm_driver, &config);
  if (ret) {
    LOG_ERR("Error initializing AML algorithm");
    return ret;
  }

  /* Set FSR */
  ret |= inv_imu_set_accel_fsr(&icm_driver, ACCEL_FSR_G);
  ret |= inv_imu_set_gyro_fsr(&icm_driver, GYRO_FSR_DPS);

  /* Set frequencies */
  ret |= inv_imu_set_accel_frequency(&icm_driver, ACCEL_FREQ);
  ret |= inv_imu_set_gyro_frequency(&icm_driver, GYRO_FREQ);


  if (enabled) {
    imu_enable();
  } else {
    imu_disable();
  }

  if (imu_int.port && !gpio_is_ready_dt(&imu_int)) {
    LOG_ERR("Error: IMU INT %s is not ready", imu_int.port->name);
    return -3;
  }

  ret = gpio_pin_configure_dt(&imu_int, GPIO_INPUT);
  if (ret) {
    LOG_ERR("Error %d: failed to configure %s pin %d", ret, imu_int.port->name, imu_int.pin);
    return ret;
  }

  ret = gpio_pin_interrupt_configure_dt(&imu_int, GPIO_INT_EDGE_TO_ACTIVE);
  if (ret) {
    LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", ret, imu_int.port->name, imu_int.pin);
    return ret;
  }

  gpio_init_callback(&imu_int_cb_data, on_data_present, BIT(imu_int.pin));
  gpio_add_callback(imu_int.port, &imu_int_cb_data);

  if (led.port && !gpio_is_ready_dt(&led)) {
    LOG_ERR("Error %d: LED device %s is not ready; ignoring it", ret, led.port->name);
    led.port = NULL;
  }

  if (led.port) {
    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
    if (ret) {
      LOG_ERR("Error %d: failed to configure LED device %s pin %d", ret, led.port->name, led.pin);
      led.port = NULL;
    } else {
      LOG_INF("Set up LED at %s pin %d", led.port->name, led.pin);
      gpio_pin_set_dt(&led, 0);
    }
  }

  if (!gpio_is_ready_dt(&button1)) {
    LOG_ERR("Could not configure button1");
    return -1;
  }

  if (!device_is_ready(dev_i2c.bus)) {
    LOG_ERR("I2C bus %s is not ready!", dev_i2c.bus->name);
    return -1;
  }

  ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
  if (ret) {
    LOG_ERR("Could not configure gpio input");
    return ret;
  }

  ret = gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_TO_ACTIVE);
  if (ret) {
    LOG_ERR("Could not configure interrupt");
    return ret;
  }

  k_thread_create(
    &imu_thread_data, imu_stack_area,
    K_THREAD_STACK_SIZEOF(imu_stack_area), imu_thread_worker,
    NULL, NULL, NULL,
    CONFIG_IMU_THREAD_PRIORITY, 0, K_NO_WAIT
  );

  return 0;
}
