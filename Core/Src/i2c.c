#include "i2c.h"
#include "main.h"   /* Error_Handler() 的声明 */

I2C_HandleTypeDef hi2c2;

/* 恢复时序用的粗延时。72MHz 下约 5us，对应约 100kHz 的手动时钟，
   比 I2C2 正常工作的 50kHz 快，但从设备只是被动移位，不影响恢复。 */
#define I2C_RECOVER_DELAY()   do { for (volatile uint32_t _i = 0; _i < 120; _i++) { __NOP(); } } while (0)

/**
  * @brief 释放被从设备卡住的 I2C2 总线
  *
  * STM32F1 的 I2C 外设有个已知缺陷：如果在 HAL_I2C_Init() 之前 SDA 已经
  * 被从设备拉低，BUSY 标志会被置起且无法自行清除，之后所有传输都超时。
  *
  * 触发场景：通过 SWD 烧录或复位 MCU 时，从设备没有被复位（它没有复位
  * 引脚，电源也没断）。若 MCU 正好停在一次读传输中间，从设备会继续把
  * SDA 按在低电平等待时钟，总线就这么僵住了。表现是上电后扫不到任何
  * 设备，必须手动按一次复位键才好。
  *
  * 做法：把 PB10/PB11 临时当普通 GPIO，手动敲时钟把从设备卡住的那个
  * 字节移完，等它释放 SDA，再补一个 STOP 时序让从设备回到空闲状态。
  *
  * @note 必须在 MX_I2C2_Init() 之前调用。
  * @retval None
  */
void I2C2_BusRecover(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* 先把外设关掉，否则它会跟我们抢这两个引脚 */
  __HAL_RCC_I2C2_CLK_ENABLE();
  hi2c2.Instance = I2C2;
  __HAL_I2C_DISABLE(&hi2c2);

  /* PB10=SCL、PB11=SDA 切成开漏输出。开漏只能拉低不能拉高，
     高电平靠上拉电阻，这样即使从设备正拉低也不会出现电流对冲。 */
  GPIO_InitStruct.Pin   = GPIO_PIN_10 | GPIO_PIN_11;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull  = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* 两条线都放开，让上拉把它们拉高 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10 | GPIO_PIN_11, GPIO_PIN_SET);
  I2C_RECOVER_DELAY();

  /* 最多敲 16 个时钟。一个字节 8 位加 ACK 是 9 个，16 个足够把任何
     卡住的传输走完；SDA 一旦回到高电平就说明从设备松手了，提前退出。 */
  for (uint8_t i = 0; i < 16; i++)
  {
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_SET)
    {
      break;
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
    I2C_RECOVER_DELAY();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
    I2C_RECOVER_DELAY();
  }

  /* 补一个 STOP：SCL 为高时把 SDA 由低拉高。
     从设备据此认为本次传输结束，回到空闲状态。 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
  I2C_RECOVER_DELAY();
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
  I2C_RECOVER_DELAY();
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);
  I2C_RECOVER_DELAY();

  /* 复位外设，清掉可能已经置起的 BUSY 标志。
     引脚会在随后 HAL_I2C_Init() -> HAL_I2C_MspInit() 里切回 AF_OD。 */
  __HAL_RCC_I2C2_FORCE_RESET();
  I2C_RECOVER_DELAY();
  __HAL_RCC_I2C2_RELEASE_RESET();
}

/**
  * @brief I2C2 Initialization Function (PB10/PB11)
  * @param None
  * @retval None
  */
void MX_I2C2_Init(void)
{
  /* 初始化外设前先确保总线是空闲的，否则 F1 的 BUSY 标志会锁死 */
  I2C2_BusRecover();

  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 50000;  // 恢复为50kHz，跟标准库一致
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C MSP Initialization
  * @param hi2c: I2C handle pointer
  * @retval None
  */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hi2c->Instance==I2C2)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C2 GPIO Configuration
    PB10     ------> I2C2_SCL
    PB11     ------> I2C2_SDA
    */
    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;  // 使能内部上拉电阻
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_I2C2_CLK_ENABLE();
  }
}

/**
  * @brief I2C MSP De-Initialization
  * @param hi2c: I2C handle pointer
  * @retval None
  */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
  if(hi2c->Instance==I2C2)
  {
    /* Peripheral clock disable */
    __HAL_RCC_I2C2_CLK_DISABLE();

    /**I2C2 GPIO Configuration
    PB10     ------> I2C2_SCL
    PB11     ------> I2C2_SDA
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_11);
  }
}
