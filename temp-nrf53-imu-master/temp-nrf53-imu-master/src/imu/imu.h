#ifndef _IMU_H
#define _IMU_H

#include <stdbool.h>

#define ACCEL_FSR_G ACCEL_CONFIG0_FS_SEL_16g
#define GYRO_FSR_DPS GYRO_CONFIG0_FS_SEL_2000dps

#define ACCEL_FREQ ACCEL_CONFIG0_ODR_100_HZ
#define GYRO_FREQ GYRO_CONFIG0_ODR_100_HZ

typedef void (*imu_on_data_received)(uint8_t, uint8_t);

static int32_t accel_mounting_matrix[9] = {0, -1, 0, -1, 0, 0, 0, 0, 1};
static int32_t gyro_mounting_matrix[9] = {0, 1, 0, 1, 0, 0, 0, 0, -1};

int imu_device_init(bool enabled, imu_on_data_received callback);
void imu_switch_status();

#endif
