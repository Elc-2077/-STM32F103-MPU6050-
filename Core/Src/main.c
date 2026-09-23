/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "OLED.h"
#include "MPU6050.h"
#include "Attitude.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/* ==================== 姿态显示参数 ==================== */
#define PI_F        3.14159265f
#define DEG2RAD     (PI_F / 180.0f)

#define AI_CX       31      /* 仪表圆心（左侧），右侧留给文字 */
#define AI_CY       32
#define AI_R        30      /* 仪表半径 */
#define AI_PSCALE   0.60f   /* 俯仰刻度：像素/度，30px 约覆盖 ±50° */

#define BL_R        28      /* 气泡水平仪外圈 */
#define BL_SCALE    0.80f   /* 像素/度 */
#define BL_MAX      22.0f   /* 小球最大偏移 */

#define TXT_X       72      /* 右侧文字区起始列 */

/* 把 0.1° 为单位的整数格式化成带一位小数的字符串。
   newlib-nano 默认不链接浮点 printf，所以手工拆整数部分和小数位。
   负数不能直接用 /10 和 %10，会丢掉 -0.x 的符号，所以先取绝对值。 */
static void Fmt10(char *out, char label, int16_t v10)
{
  int16_t a = v10 < 0 ? -v10 : v10;
  sprintf(out, "%c%s%d.%d", label, v10 < 0 ? "-" : "", a / 10, a % 10);
}

/* 端点先裁剪再画线，避免传入 uint8_t 时回绕 */
static void AI_Line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
  if (x0 < 0) x0 = 0; else if (x0 > 127) x0 = 127;
  if (x1 < 0) x1 = 0; else if (x1 > 127) x1 = 127;
  if (y0 < 0) y0 = 0; else if (y0 > 63) y0 = 63;
  if (y1 < 0) y1 = 0; else if (y1 > 63) y1 = 63;
  OLED_DrawLine((uint8_t)x0, (uint8_t)y0, (uint8_t)x1, (uint8_t)y1);
}

/**
  * @brief 画飞机姿态仪（人工地平仪）
  * @param pitch 俯仰角，屏幕后倒为正，地平线下移
  * @param roll  横滚角，屏幕平面内旋转，地平线同步倾斜
  */
static void Draw_AttitudeIndicator(float pitch, float roll)
{
  float rr = roll * DEG2RAD;
  float cs = cosf(rr);
  float sn = sinf(rr);
  float off = pitch * AI_PSCALE;   /* 地平线沿“地面法线”的偏移 */
  int16_t x, y;
  int16_t i;

  /* 地平线方向 (cs, sn)，地面侧法线 (-sn, cs) */

  /* --- 地面侧填充网点，区分地面/天空（竖直状态下x-z轴互换） --- */
  for (y = -AI_R; y <= AI_R; y++)
  {
    float halfw = (float)(AI_R * AI_R - y * y);
    int16_t hw;
    float d;

    if (halfw <= 0.0f) continue;
    hw = (int16_t)sqrtf(halfw);

    /* 到地平线的有符号距离，行内沿 x 线性递增 */
    d = (-sn) * (float)(-hw) + cs * (float)y - off;

    for (x = -hw; x <= hw; x++, d += (-sn))
    {
      if (d > 0.0f && (((x + y) & 1) == 0))
      {
        OLED_SetPixel((uint8_t)(AI_CX + x), (uint8_t)(AI_CY + y), 1);
      }
    }
  }

  /* --- 外框 --- */
  OLED_DrawCircle(AI_CX, AI_CY, AI_R);

  /* --- 地平线：圆内弦长 --- */
  {
    float half = (float)(AI_R * AI_R) - off * off;
    if (half > 0.0f)
    {
      float L = sqrtf(half);
      float bx = AI_CX + (-sn) * off;
      float by = AI_CY + cs * off;
      AI_Line((int16_t)(bx - cs * L), (int16_t)(by - sn * L),
              (int16_t)(bx + cs * L), (int16_t)(by + sn * L));
    }
  }

  /* --- 俯仰刻度梯：±10°/±20° --- */
  for (i = 0; i < 4; i++)
  {
    const int8_t marks[4] = {-20, -10, 10, 20};
    float o = (pitch - (float)marks[i]) * AI_PSCALE;
    float half = (float)(AI_R * AI_R) - o * o;
    float len = (marks[i] % 20 == 0) ? 9.0f : 5.0f;   /* 20°长，10°短 */
    float bx, by;

    if (half <= 0.0f) continue;
    bx = AI_CX + (-sn) * o;
    by = AI_CY + cs * o;

    /* 中间留缺口，左右各一段 */
    AI_Line((int16_t)(bx - cs * len), (int16_t)(by - sn * len),
            (int16_t)(bx - cs * 3.0f), (int16_t)(by - sn * 3.0f));
    AI_Line((int16_t)(bx + cs * 3.0f), (int16_t)(by + sn * 3.0f),
            (int16_t)(bx + cs * len), (int16_t)(by + sn * len));
  }

  /* --- 固定倾角刻度 0/±30/±60，画在圆内侧 --- */
  for (i = 0; i < 5; i++)
  {
    const int8_t bank[5] = {-60, -30, 0, 30, 60};
    float b = (float)bank[i] * DEG2RAD;
    float dx = sinf(b);
    float dy = -cosf(b);
    float r1 = (float)AI_R - 1.0f;
    float r0 = r1 - ((bank[i] == 0) ? 5.0f : 3.0f);
    AI_Line((int16_t)(AI_CX + dx * r0), (int16_t)(AI_CY + dy * r0),
            (int16_t)(AI_CX + dx * r1), (int16_t)(AI_CY + dy * r1));
  }

  /* --- 随横滚转动的指针（三角形），天空方向 (sn, -cs) --- */
  {
    float r1 = (float)AI_R - 7.0f;
    float r2 = (float)AI_R - 1.0f;
    float ax = AI_CX + sn * r2;
    float ay = AI_CY - cs * r2;
    float b1x = AI_CX + sn * r1 + cs * 3.0f;
    float b1y = AI_CY - cs * r1 + sn * 3.0f;
    float b2x = AI_CX + sn * r1 - cs * 3.0f;
    float b2y = AI_CY - cs * r1 - sn * 3.0f;
    AI_Line((int16_t)ax, (int16_t)ay, (int16_t)b1x, (int16_t)b1y);
    AI_Line((int16_t)ax, (int16_t)ay, (int16_t)b2x, (int16_t)b2y);
    AI_Line((int16_t)b1x, (int16_t)b1y, (int16_t)b2x, (int16_t)b2y);
  }

  /* --- 固定飞机符号：先清出背景，再画，避免和网点糊在一起 --- */
  for (y = AI_CY - 4; y <= AI_CY + 4; y++)
  {
    for (x = AI_CX - 15; x <= AI_CX + 15; x++)
    {
      OLED_SetPixel((uint8_t)x, (uint8_t)y, 0);
    }
  }
  AI_Line(AI_CX - 14, AI_CY, AI_CX - 5, AI_CY);        /* 左机翼 */
  AI_Line(AI_CX + 5, AI_CY, AI_CX + 14, AI_CY);        /* 右机翼 */
  AI_Line(AI_CX - 14, AI_CY, AI_CX - 14, AI_CY + 3);   /* 左翼尖 */
  AI_Line(AI_CX + 14, AI_CY, AI_CX + 14, AI_CY + 3);   /* 右翼尖 */
  OLED_SetPixel(AI_CX, AI_CY, 1);                      /* 机身中心 */
  OLED_SetPixel(AI_CX - 1, AI_CY, 1);
  OLED_SetPixel(AI_CX + 1, AI_CY, 1);
}

/**
  * @brief 画气泡水平仪（平放模式）
  */
static void Draw_BubbleLevel(float ax_deg, float ay_deg)
{
  /* 小球往低的一侧滚：机体 +x 抬高时球向 -x 走 */
  float ox = -ax_deg * BL_SCALE;
  float oy =  ay_deg * BL_SCALE;
  float d = sqrtf(ox * ox + oy * oy);
  int16_t bx, by;

  if (d > BL_MAX)
  {
    ox = ox * BL_MAX / d;
    oy = oy * BL_MAX / d;
  }
  bx = AI_CX + (int16_t)ox;
  by = AI_CY + (int16_t)oy;

  OLED_DrawCircle(AI_CX, AI_CY, BL_R);
  OLED_DrawCircle(AI_CX, AI_CY, 6);          /* 居中容差圈 */
  AI_Line(AI_CX - 9, AI_CY, AI_CX + 9, AI_CY);
  AI_Line(AI_CX, AI_CY - 9, AI_CX, AI_CY + 9);
  OLED_FillCircle((uint8_t)bx, (uint8_t)by, 4);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();

  /* Configure PC13 as output for LED diagnostic */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* Blink LED fast 3 times - program started */
  for(int i = 0; i < 3; i++) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_Delay(200);
  }

  /* Hardware I2C2 initialized */
  HAL_Delay(100);

  /* 测试 I2C 总线上的设备 */
  uint8_t i2c_found = 0;

  /* 扫描 OLED (0x78) */
  if (HAL_I2C_IsDeviceReady(&hi2c2, 0x78, 3, 100) == HAL_OK) {
    i2c_found++;
  }

  /* 扫描 MPU6050 (0xD0) */
  if (HAL_I2C_IsDeviceReady(&hi2c2, 0xD0, 3, 100) == HAL_OK) {
    i2c_found++;
  }

  /* Blink LED according to found devices */
  for(int i = 0; i < i2c_found; i++) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_Delay(300);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_Delay(300);
  }

  /* If no device found, blink fast continuously */
  if (i2c_found == 0) {
    while(1) {
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
      HAL_Delay(100);
    }
  }

  HAL_Delay(500);

  /* Initialize OLED */
  OLED_Init();
  HAL_Delay(100);
  OLED_Clear();

  /* Blink LED 1 long time - OLED initialized */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  HAL_Delay(1000);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  HAL_Delay(500);

  /* 显示初始化信息 */
  OLED_ShowString(1, 1, "Initializing...");
  HAL_Delay(500);

  /* Initialize MPU6050 */
  uint8_t mpu_result = MPU6050_Init();

  if (mpu_result != 0)
  {
    OLED_Clear();
    OLED_ShowString(1, 1, "MPU6050 Error!");

    /* 显示实际读到的ID值用于调试 */
    uint8_t actual_id = MPU6050_ReadReg(MPU6050_WHO_AM_I);
    OLED_ShowString(2, 1, "ID:");
    OLED_ShowNum(2, 4, actual_id, 2);

    OLED_ShowString(3, 1, "SCL->PB10");
    OLED_ShowString(4, 1, "SDA->PB11");

    /* Blink LED fast continuously - MPU6050 error */
    while (1)
    {
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
      HAL_Delay(200);
    }
  }

  /* Blink LED 4 times - MPU6050 OK */
  for(int i = 0; i < 4; i++) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_Delay(150);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_Delay(150);
  }

  OLED_Clear();
  OLED_ShowString(1, 1, "MPU6050 OK!");
  HAL_Delay(500);

  /* 陀螺仪零偏标定：必须静止放置，否则姿态会缓慢漂移 */
  OLED_Clear();
  OLED_ShowString(1, 1, "Calibrating");
  OLED_ShowString(2, 1, "Keep still...");
  Attitude_Init();
  Attitude_CalibrateGyro(200);
  OLED_Clear();

  /* 预热滤波器，避免开机瞬间从 0 冲到实际角度 */
  for (int i = 0; i < 60; i++)
  {
    Attitude_Update();
    HAL_Delay(5);
  }

  StickyInt_t sPitch = {0}, sRoll = {0}, sTilt = {0};
  StickyInt_t sAx = {0}, sAy = {0}, sTf = {0};
  uint32_t last_draw = HAL_GetTick();
  char buf[12];

  /* Infinite loop */
  while (1)
  {
    /* 高频采样推进滤波器（约 200Hz），显示按 50ms 刷新 */
    Attitude_Update();

    if ((HAL_GetTick() - last_draw) < 50)
    {
      continue;
    }
    last_draw = HAL_GetTick();

    OLED_ClearBuffer();

    if (Attitude.Mode == ATT_MODE_FLAT)   /* 平放：气泡水平仪 */
    {
      Draw_BubbleLevel(Attitude.AngleX, Attitude.AngleY);

      OLED_ShowStringInBuffer(TXT_X, 0, "LEVL");
      Fmt10(buf, 'X', Attitude_StickyInt(&sAx, Attitude.AngleX * 10.0f, 2.0f));
      OLED_ShowStringInBuffer(TXT_X, 16, buf);
      Fmt10(buf, 'Y', Attitude_StickyInt(&sAy, Attitude.AngleY * 10.0f, 2.0f));
      OLED_ShowStringInBuffer(TXT_X, 32, buf);
      Fmt10(buf, 'T', Attitude_StickyInt(&sTf, Attitude.TiltFlat * 10.0f, 2.0f));
      OLED_ShowStringInBuffer(TXT_X, 48, buf);
    }
    else                                  /* 竖直：飞机姿态仪 */
    {
      Draw_AttitudeIndicator(Attitude.Pitch, Attitude.Roll);

      OLED_ShowStringInBuffer(TXT_X, 0, "VERT");
      Fmt10(buf, 'P', Attitude_StickyInt(&sPitch, Attitude.Pitch * 10.0f, 2.0f));
      OLED_ShowStringInBuffer(TXT_X, 16, buf);
      Fmt10(buf, 'R', Attitude_StickyInt(&sRoll, Attitude.Roll * 10.0f, 2.0f));
      OLED_ShowStringInBuffer(TXT_X, 32, buf);
      /* 偏离竖直的总倾角 */
      Fmt10(buf, 'T', Attitude_StickyInt(&sTilt, Attitude.Tilt * 10.0f, 2.0f));
      OLED_ShowStringInBuffer(TXT_X, 48, buf);
    }

    OLED_UpdateDisplay();
  }
}
  
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
}
#endif /* USE_FULL_ASSERT */
