#ifndef __OLED_H
#define __OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

#define OLED_ADDRESS 0x78  // OLED I2C 地址 (SA0=GND: 0x78, SA0=VCC: 0x7A)

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowFloat(uint8_t Line, uint8_t Column, float Number, uint8_t IntLen, uint8_t FraLen);
void OLED_WriteDataBuffer(const uint8_t *Data, uint16_t Len);
void OLED_ShowStringShift(uint8_t Line, const char *String, uint16_t Offset);

/* 图形绘制函数 */
void OLED_SetPixel(uint8_t X, uint8_t Y, uint8_t Value);
void OLED_DrawCircle(uint8_t X0, uint8_t Y0, uint8_t Radius);
void OLED_FillCircle(uint8_t X0, uint8_t Y0, uint8_t Radius);
void OLED_DrawLine(uint8_t X0, uint8_t Y0, uint8_t X1, uint8_t Y1);
void OLED_ClearBuffer(void);
void OLED_UpdateDisplay(void);
void OLED_ShowCharInBuffer(uint8_t X, uint8_t Y, char Char);
void OLED_ShowStringInBuffer(uint8_t X, uint8_t Y, const char *String);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
