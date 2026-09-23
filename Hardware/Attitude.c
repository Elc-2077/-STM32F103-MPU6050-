#include "Attitude.h"
#include "MPU6050.h"
#include <math.h>

/* ==================== 可调参数 ==================== */

#define MED_WIN         5       /* 中值滤波窗口，去除单点尖峰 */
#define TAU_COMP        0.05f   /* 互补滤波时间常数[s]，越大越稳越迟钝 */
#define TAU_FLAT        0.15f   /* 平放模式加速度低通时间常数[s] */
#define TAU_ACC         0.02f   /* 加速度预滤波，互补滤波本身已是低通，这里只去残余噪声 */

/* 加速度计静态零偏[LSB]，平放时测出来填进去。
   测法：板子平放静止，把此处清零烧一次，读 X/Y 两个倾角，
   偏差 1° 约对应 286 LSB（16384 * sin1°），按符号填入即可。 */
#define OFF_AX          0.0f
#define OFF_AY          0.0f
#define OFF_AZ          0.0f

/* 陀螺仪零偏在线估计：积分反馈增益[1/s]。越大收敛越快但越容易被扰动带跑 */
#define BIAS_GAIN       0.03f
#define BIAS_LIMIT      5.0f    /* 零偏估计上限[dps]，防积分饱和 */

/* 加速度可信度门限：|a| 偏离 1g 超过 DEV_MAX 就完全不信（纯陀螺仪外推） */
#define DEV_MIN         0.04f   /* 偏离 4% 以内完全可信 */
#define DEV_MAX         0.25f   /* 偏离 25% 以上完全不可信 */
#define STILL_RATE      2.0f    /* 角速度低于此值[dps]才学习零偏 */

/* 模式切换回差：|uz| 大于 ENTER 进平放，小于 EXIT 进竖直 */
#define FLAT_ENTER      0.80f
#define FLAT_EXIT       0.65f

/* 量程换算：陀螺仪 ±500dps，加速度 ±2g */
#define GYRO_LSB        65.5f
#define ACC_LSB         16384.0f

/* ==================== 轴向重映射 ====================
   机体系定义：x = 屏幕向右，y = 屏幕向上，z = 屏幕朝外（法线）。
   当前为直通，即机体系 = MPU 原始轴。

   当前配置：X 与 Y 互换，平放和竖直两个模式共用。
   成对交换两条轴时给第三条加了负号，否则左右手系翻转，
   姿态仪的旋转方向会反，陀螺仪也会和加速度计在互补滤波里对着干。
   若竖直站立时 Roll 显示 ±180 而不是 0，把下面两行的符号对调：
     #define UP_X(rx, ry, rz)   (ry)
     #define UP_Y(rx, ry, rz)   (-(rx))
   加速度和陀螺仪必须用同一套映射。 */
#define UP_X(rx, ry, rz)   (-(ry))
#define UP_Y(rx, ry, rz)   (rx)
#define UP_Z(rx, ry, rz)   (-(rz))

/* 机体角速度 -> 欧拉角速度投影（见 Attitude_Update 里的推导注释）。
   若姿态仪动作方向整体相反，把这两个宏取反 */
#define WX(gx, gy, gz)   (gx)
#define WY(gx, gy, gz)   (gy)
#define WZ(gx, gy, gz)   (gz)

#define RAD2DEG         57.29578f
#define DEG2RAD         0.01745329f

Attitude_t Attitude;

/* ==================== 内部状态 ==================== */

static int16_t med_buf[6][MED_WIN];
static uint8_t med_idx;
static uint8_t med_primed;

static float acc_x, acc_y, acc_z;       /* 低通后的加速度（LSB） */
static float gyro_bias[3];              /* 开机标定的静态零偏[LSB] */
static float bias_p, bias_r;            /* 在线估计的欧拉角速度残余零偏[dps] */
static float pitch_f, roll_f;           /* 互补滤波输出 */
static float flat_x, flat_y, flat_z;    /* 平放模式慢速低通 */
static uint32_t last_tick;
static uint8_t primed;

/* 5 点中值 */
static int16_t median5(const int16_t *w)
{
    int16_t a[MED_WIN];
    uint8_t i, j;
    for (i = 0; i < MED_WIN; i++) a[i] = w[i];
    for (i = 1; i < MED_WIN; i++)
    {
        int16_t k = a[i];
        for (j = i; j > 0 && a[j - 1] > k; j--) a[j] = a[j - 1];
        a[j] = k;
    }
    return a[MED_WIN / 2];
}

/* 角度差归一到 -180..180，供 Roll 跨 ±180 使用 */
static float wrap180(float a)
{
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

void Attitude_Init(void)
{
    med_idx = 0;
    med_primed = 0;
    primed = 0;
    bias_p = 0.0f;
    bias_r = 0.0f;
    last_tick = HAL_GetTick();
}

void Attitude_CalibrateGyro(uint16_t samples)
{
    int16_t ax, ay, az, gx, gy, gz;
    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
    uint16_t i;

    if (samples == 0) samples = 1;

    for (i = 0; i < samples; i++)
    {
        MPU6050_GetData(&ax, &ay, &az, &gx, &gy, &gz);
        sx += (float)gx;
        sy += (float)gy;
        sz += (float)gz;
        HAL_Delay(5);
    }

    gyro_bias[0] = sx / (float)samples;
    gyro_bias[1] = sy / (float)samples;
    gyro_bias[2] = sz / (float)samples;

    /* 标定期间的读数不参与滤波 */
    Attitude_Init();
}

int16_t Attitude_StickyInt(StickyInt_t *s, float value, float band)
{
    float diff;

    if (!s->Primed)
    {
        s->Value = (int16_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
        s->Primed = 1;
        return s->Value;
    }

    diff = value - (float)s->Value;
    if (diff > band || diff < -band)
    {
        s->Value = (int16_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
    }
    return s->Value;
}

void Attitude_Update(void)
{
    int16_t raw[6];
    int16_t m[6];
    uint8_t i;
    uint32_t now;
    float dt, a, norm, ux, uy, uz;
    float bax, bay, baz;                /* 重映射后的机体系加速度 */
    float gx, gy, gz, roll_rate, pitch_rate;
    float pitch_acc, roll_acc, inplane;

    MPU6050_GetData(&raw[0], &raw[1], &raw[2], &raw[3], &raw[4], &raw[5]);

    /* --- 第一级：中值滤波，剔除 I2C 误码和机械冲击造成的单点尖峰 --- */
    if (!med_primed)
    {
        for (i = 0; i < 6; i++)
        {
            uint8_t k;
            for (k = 0; k < MED_WIN; k++) med_buf[i][k] = raw[i];
        }
        med_primed = 1;
    }
    else
    {
        for (i = 0; i < 6; i++) med_buf[i][med_idx] = raw[i];
    }
    med_idx = (uint8_t)((med_idx + 1) % MED_WIN);
    for (i = 0; i < 6; i++) m[i] = median5(med_buf[i]);

    /* --- 时间步长，滤波系数按实际 dt 计算，刷新率变化不影响手感 --- */
    now = HAL_GetTick();
    dt = (float)(now - last_tick) * 0.001f;
    last_tick = now;
    if (dt <= 0.0f) dt = 0.001f;
    if (dt > 0.2f) dt = 0.2f;        /* 卡顿保护 */

    /* --- 轴向重映射：原始轴 -> 机体系（X/Y 互换），两个模式共用。
           先减加速度计静态零偏 --- */
    bax = UP_X((float)m[0] - OFF_AX, (float)m[1] - OFF_AY, (float)m[2] - OFF_AZ);
    bay = UP_Y((float)m[0] - OFF_AX, (float)m[1] - OFF_AY, (float)m[2] - OFF_AZ);
    baz = UP_Z((float)m[0] - OFF_AX, (float)m[1] - OFF_AY, (float)m[2] - OFF_AZ);

    /* --- 第二级：加速度一阶低通，得到重力方向 --- */
    if (!primed)
    {
        acc_x = bax;
        acc_y = bay;
        acc_z = baz;
        flat_x = acc_x; flat_y = acc_y; flat_z = acc_z;
    }
    else
    {
        a = dt / (TAU_ACC + dt);
        acc_x += a * (bax - acc_x);
        acc_y += a * (bay - acc_y);
        acc_z += a * (baz - acc_z);

        a = dt / (TAU_FLAT + dt);
        flat_x += a * (bax - flat_x);
        flat_y += a * (bay - flat_y);
        flat_z += a * (baz - flat_z);
    }

    /* 机体系下的“天顶”单位向量 */
    norm = sqrtf(acc_x * acc_x + acc_y * acc_y + acc_z * acc_z);
    if (norm < 1000.0f) norm = 1000.0f;     /* 失重/异常时兜底 */
    ux = acc_x / norm;
    uy = acc_y / norm;
    uz = acc_z / norm;

    /* --- 竖直模式角度：加速度解算 --- */
    inplane = sqrtf(ux * ux + uy * uy);
    pitch_acc = atan2f(uz, inplane) * RAD2DEG;    /* 屏幕后倒为正 */
    roll_acc  = atan2f(ux, uy) * RAD2DEG;         /* 屏幕平面内旋转 */

    /* --- 第三级：互补滤波，陀螺仪给高频、加速度给低频 ---
       零偏是在原始轴上标定的，所以先减零偏再做同样的轴向重映射 */
    {
        float rgx = ((float)m[3] - gyro_bias[0]) / GYRO_LSB;
        float rgy = ((float)m[4] - gyro_bias[1]) / GYRO_LSB;
        float rgz = ((float)m[5] - gyro_bias[2]) / GYRO_LSB;
        float bgx = UP_X(rgx, rgy, rgz);
        float bgy = UP_Y(rgx, rgy, rgz);
        float bgz = UP_Z(rgx, rgy, rgz);
        gx = WX(bgx, bgy, bgz);
        gy = WY(bgx, bgy, bgz);
        gz = WZ(bgx, bgy, bgz);
    }

    /* 机体角速度 -> 欧拉角速度投影。
       天顶向量在世界系固定，在机体系里 du/dt = -w x u，代入
         ux = cos(p)sin(r), uy = cos(p)cos(r), uz = sin(p)
       解得：
         dp/dt = wy*sin(r) - wx*cos(r)
         dr/dt = wz - tan(p)*(wx*sin(r) + wy*cos(r))
       原来的固定映射 (dp=-wx, dr=wz) 只是 p=0,r=0 的特例，
       一旦横滚或俯仰离开零点就会串轴，倾得越多误差越大。 */
    {
        float rp = roll_f * DEG2RAD;
        float sr = sinf(rp);
        float cr = cosf(rp);
        float tp = tanf(pitch_f * DEG2RAD);

        /* p 接近 ±90° 时 tan 发散（万向节锁），此处正好是切平放模式的区域，限幅即可 */
        if (tp > 4.0f) tp = 4.0f;
        if (tp < -4.0f) tp = -4.0f;

        pitch_rate = gy * sr - gx * cr;
        roll_rate  = gz - tp * (gx * sr + gy * cr);
    }

    /* 减掉在线估计的残余零偏 */
    pitch_rate -= bias_p;
    roll_rate  -= bias_r;

    if (!primed)
    {
        pitch_f = pitch_acc;
        roll_f  = roll_acc;
        primed  = 1;
    }
    else
    {
        /* 加速度可信度：|a| 偏离 1g 说明混进了线加速度，此时应该少信它。
           静止时 trust=1 退化成原来的互补滤波，甩动时 trust->0 变成纯陀螺仪外推，
           这样晃动过程中地平线不会被拽歪，停下来又能自动收敛回去。 */
        float dev = norm / ACC_LSB - 1.0f;
        float trust;
        float err_p, err_r;

        if (dev < 0.0f) dev = -dev;
        if (dev <= DEV_MIN)      trust = 1.0f;
        else if (dev >= DEV_MAX) trust = 0.0f;
        else                     trust = (DEV_MAX - dev) / (DEV_MAX - DEV_MIN);

        a = TAU_COMP / (TAU_COMP + dt * trust);   /* 陀螺仪权重，trust=0 时 a=1 */

        /* 先按陀螺仪外推，再用加速度拉回 */
        pitch_f += pitch_rate * dt;
        roll_f   = wrap180(roll_f + roll_rate * dt);

        err_p = pitch_acc - pitch_f;
        err_r = wrap180(roll_acc - roll_f);

        pitch_f += (1.0f - a) * err_p;
        roll_f   = wrap180(roll_f + (1.0f - a) * err_r);

        /* 陀螺仪残余零偏在线估计：稳态下角度误差正比于零偏，
           把误差积分回角速度，温漂也能自动跟掉。
           只在接近静止且加速度可信时学习，否则会把机动当成零偏学进去。 */
        {
            float wmag = sqrtf(gx * gx + gy * gy + gz * gz);
            if (trust > 0.99f && wmag < STILL_RATE)
            {
                bias_p -= BIAS_GAIN * err_p * dt;
                bias_r -= BIAS_GAIN * err_r * dt;

                if (bias_p >  BIAS_LIMIT) bias_p =  BIAS_LIMIT;
                if (bias_p < -BIAS_LIMIT) bias_p = -BIAS_LIMIT;
                if (bias_r >  BIAS_LIMIT) bias_r =  BIAS_LIMIT;
                if (bias_r < -BIAS_LIMIT) bias_r = -BIAS_LIMIT;
            }
        }
    }

    Attitude.Pitch = pitch_f;
    Attitude.Roll  = roll_f;

    /* 偏离竖直站立的总倾角：机体 +Y 与天顶的夹角。
       由融合后的 pitch/roll 反算（uy = cos(p)cos(r)），而不是直接用加速度，
       这样能一起吃到陀螺仪融合的抗噪，不会像原来那样单独抖。 */
    {
        float cy = cosf(pitch_f * DEG2RAD) * cosf(roll_f * DEG2RAD);
        if (cy > 1.0f) cy = 1.0f;
        if (cy < -1.0f) cy = -1.0f;
        Attitude.Tilt = acosf(cy) * RAD2DEG;
    }

    /* --- 平放模式角度：慢速低通即可，不需要陀螺仪 --- */
    norm = sqrtf(flat_x * flat_x + flat_y * flat_y + flat_z * flat_z);
    if (norm < 1000.0f) norm = 1000.0f;
    {
        float fx = flat_x / norm;
        float fy = flat_y / norm;
        float fz = flat_z / norm;
        float az = fz < 0.0f ? -fz : fz;

        if (fx > 1.0f) fx = 1.0f;
        if (fx < -1.0f) fx = -1.0f;
        if (fy > 1.0f) fy = 1.0f;
        if (fy < -1.0f) fy = -1.0f;
        if (az > 1.0f) az = 1.0f;

        Attitude.AngleX = asinf(fx) * RAD2DEG;
        Attitude.AngleY = asinf(fy) * RAD2DEG;
        Attitude.TiltFlat = acosf(az) * RAD2DEG;
    }

    /* --- 模式判别，带回差防止临界点来回跳 --- */
    {
        float az = uz < 0.0f ? -uz : uz;
        if (Attitude.Mode == ATT_MODE_FLAT)
        {
            if (az < FLAT_EXIT) Attitude.Mode = ATT_MODE_UPRIGHT;
        }
        else
        {
            if (az > FLAT_ENTER) Attitude.Mode = ATT_MODE_FLAT;
        }
    }
}
