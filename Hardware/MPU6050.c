#include "MPU6050.h"
#include "i2c.h"

void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data)
{
    uint8_t data[2] = {RegAddress, Data};
    HAL_I2C_Master_Transmit(&hi2c2, MPU6050_ADDRESS, data, 2, 1000);
}

uint8_t MPU6050_ReadReg(uint8_t RegAddress)
{
    uint8_t data;
    HAL_I2C_Master_Transmit(&hi2c2, MPU6050_ADDRESS, &RegAddress, 1, 1000);
    HAL_I2C_Master_Receive(&hi2c2, MPU6050_ADDRESS, &data, 1, 1000);
    return data;
}

uint8_t MPU6050_Init(void)
{
    uint8_t id;

    HAL_Delay(100);

    /* 先解除休眠，有些MPU6050上电后处于休眠状态 */
    MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x00);
    HAL_Delay(50);

    /* 检查设备 ID */
    id = MPU6050_ReadReg(MPU6050_WHO_AM_I);
    if (id != 0x70)
    {
        return 1;  /* 初始化失败 */
    }

    /* 采样率分频：1kHz / (1 + 4) = 200Hz，给软件滤波留足样本 */
    MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x04);

    /* 硬件低通 DLPF_CFG=3：加速度 44Hz / 陀螺仪 42Hz
       第一级抗高频抖动，剩下的交给软件中值+互补滤波，
       比 DLPF_CFG=6(5Hz) 延迟小得多 */
    MPU6050_WriteReg(MPU6050_CONFIG, 0x03);

    /* 配置陀螺仪量程：±500°/s（65.5 LSB/dps），手持姿态足够且分辨率更高 */
    MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x08);

    /* 配置加速度计量程：±2g */
    MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x00);

    return 0;  /* 初始化成功 */
}

void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ,
                     int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ)
{
    uint8_t data[14];
    uint8_t reg = MPU6050_ACCEL_XOUT_H;

    /* 使用 HAL 库从 0x3B 开始连续读取 14 字节数据 */
    HAL_I2C_Master_Transmit(&hi2c2, MPU6050_ADDRESS, &reg, 1, 1000);
    HAL_I2C_Master_Receive(&hi2c2, MPU6050_ADDRESS, data, 14, 1000);

    /* 加速度数据 */
    *AccX = (int16_t)((data[0] << 8) | data[1]);
    *AccY = (int16_t)((data[2] << 8) | data[3]);
    *AccZ = (int16_t)((data[4] << 8) | data[5]);

    /* 陀螺仪数据 */
    *GyroX = (int16_t)((data[8] << 8) | data[9]);
    *GyroY = (int16_t)((data[10] << 8) | data[11]);
    *GyroZ = (int16_t)((data[12] << 8) | data[13]);
}
