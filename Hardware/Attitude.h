#ifndef __ATTITUDE_H
#define __ATTITUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* 显示模式 */
typedef enum
{
    ATT_MODE_FLAT = 0,   /* 平放：气泡水平仪 */
    ATT_MODE_UPRIGHT     /* 竖直：飞机姿态仪 */
} Attitude_Mode;

typedef struct
{
    /* ---- 竖直模式（飞机姿态仪）---- */
    float Pitch;      /* 俯仰角 -90..+90，屏幕向后倒为正 */
    float Roll;       /* 横滚角 -180..+180，屏幕平面内逆时针为正 */
    float Tilt;       /* 偏离“竖直站立”的总倾角 0..180 */

    /* ---- 平放模式（气泡水平仪）---- */
    float AngleX;     /* 左右倾角 -90..+90 */
    float AngleY;     /* 前后倾角 -90..+90 */
    float TiltFlat;   /* 偏离水平面的总倾角 0..90 */

    Attitude_Mode Mode;
} Attitude_t;

/* 显示用防抖整数：只有偏离当前显示值足够多才跳数 */
typedef struct
{
    int16_t Value;
    uint8_t Primed;
} StickyInt_t;

extern Attitude_t Attitude;

/* 复位滤波器状态（不读传感器） */
void Attitude_Init(void);

/* 静止放置时采样陀螺仪零偏，samples 建议 200 */
void Attitude_CalibrateGyro(uint16_t samples);

/* 读一次 MPU6050 并推进滤波器，主循环里尽量高频调用 */
void Attitude_Update(void);

/* 带回差的取整，消除末位数字反复跳动。
   band 为回差幅度，单位和 value 一致（显示 0.1° 时传 2.0 即 0.2°） */
int16_t Attitude_StickyInt(StickyInt_t *s, float value, float band);

#ifdef __cplusplus
}
#endif

#endif /* __ATTITUDE_H */
