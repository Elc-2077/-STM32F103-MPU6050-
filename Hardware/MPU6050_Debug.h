#ifndef __MPU6050_DEBUG_H
#define __MPU6050_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* 测试I2C总线和MPU6050连接 */
uint8_t MPU6050_TestConnection(void);

/* 扫描I2C总线上的设备 */
void I2C_Scan(uint8_t *devices, uint8_t *count);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_DEBUG_H */
