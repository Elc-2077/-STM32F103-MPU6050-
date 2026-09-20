#include "MyI2C.h"

/* 软件I2C引脚定义 - 用于OLED */
#define SCL_GPIO_PORT   GPIOB
#define SCL_PIN         GPIO_PIN_8
#define SDA_GPIO_PORT   GPIOB
#define SDA_PIN         GPIO_PIN_9

/* 引脚操作宏 */
#define SCL_HIGH()      HAL_GPIO_WritePin(SCL_GPIO_PORT, SCL_PIN, GPIO_PIN_SET)
#define SCL_LOW()       HAL_GPIO_WritePin(SCL_GPIO_PORT, SCL_PIN, GPIO_PIN_RESET)
#define SDA_HIGH()      HAL_GPIO_WritePin(SDA_GPIO_PORT, SDA_PIN, GPIO_PIN_SET)
#define SDA_LOW()       HAL_GPIO_WritePin(SDA_GPIO_PORT, SDA_PIN, GPIO_PIN_RESET)
#define SDA_READ()      HAL_GPIO_ReadPin(SDA_GPIO_PORT, SDA_PIN)

/* 简单延时 */
static void MyI2C_Delay(void)
{
    uint8_t i = 10;
    while(i--);
}

/**
 * @brief  软件I2C初始化
 */
void MyI2C_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SCL_PIN | SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;  // 开漏输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;          // 内部上拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    SCL_HIGH();
    SDA_HIGH();
}

/**
 * @brief  I2C起始信号
 */
void MyI2C_Start(void)
{
    SDA_HIGH();
    SCL_HIGH();
    MyI2C_Delay();
    SDA_LOW();
    MyI2C_Delay();
    SCL_LOW();
}

/**
 * @brief  I2C停止信号
 */
void MyI2C_Stop(void)
{
    SDA_LOW();
    MyI2C_Delay();
    SCL_HIGH();
    MyI2C_Delay();
    SDA_HIGH();
    MyI2C_Delay();
}

/**
 * @brief  I2C发送一个字节
 */
void MyI2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (Byte & (0x80 >> i))
        {
            SDA_HIGH();
        }
        else
        {
            SDA_LOW();
        }
        MyI2C_Delay();
        SCL_HIGH();
        MyI2C_Delay();
        SCL_LOW();
    }
}

/**
 * @brief  I2C接收应答位
 */
uint8_t MyI2C_ReceiveAck(void)
{
    uint8_t AckBit;
    SDA_HIGH();
    MyI2C_Delay();
    SCL_HIGH();
    MyI2C_Delay();
    AckBit = SDA_READ();
    SCL_LOW();
    MyI2C_Delay();
    return AckBit;
}
