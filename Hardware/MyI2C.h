#ifndef __MYI2C_H
#define __MYI2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

void MyI2C_Init(void);
void MyI2C_Start(void);
void MyI2C_Stop(void);
void MyI2C_SendByte(uint8_t Byte);
uint8_t MyI2C_ReceiveAck(void);

#ifdef __cplusplus
}
#endif

#endif /* __MYI2C_H */
