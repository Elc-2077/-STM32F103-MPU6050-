#include "OLED.h"
#include "MyI2C.h"
#include "OLED_Font.h"
#include <string.h>

void OLED_WriteCommand(uint8_t Command)
{
    MyI2C_Start();
    MyI2C_SendByte(0x78);
    MyI2C_ReceiveAck();
    MyI2C_SendByte(0x00);
    MyI2C_ReceiveAck();
    MyI2C_SendByte(Command);
    MyI2C_ReceiveAck();
    MyI2C_Stop();
}

void OLED_WriteData(uint8_t Data)
{
    MyI2C_Start();
    MyI2C_SendByte(0x78);
    MyI2C_ReceiveAck();
    MyI2C_SendByte(0x40);
    MyI2C_ReceiveAck();
    MyI2C_SendByte(Data);
    MyI2C_ReceiveAck();
    MyI2C_Stop();
}

void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    OLED_WriteCommand(0xB0 | Y);
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | (X & 0x0F));
}

void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 8; j++)
    {
        OLED_SetCursor(j, 0);
        for(i = 0; i < 128; i++)
        {
            OLED_WriteData(0x00);
        }
    }
}

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i]);
    }
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);
    }
}

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}


void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + Length - i - 1, Number % 10 + '0');
        Number /= 10;
    }
}

void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
    uint8_t i;
    uint32_t Number1;
    if (Number >= 0)
    {
        OLED_ShowChar(Line, Column, '+');
        Number1 = Number;
    }
    else
    {
        OLED_ShowChar(Line, Column, '-');
        Number1 = -Number;
    }
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + Length - i, Number1 % 10 + '0');
        Number1 /= 10;
    }
}

void OLED_ShowFloat(uint8_t Line, uint8_t Column, float Number, uint8_t IntLen, uint8_t FraLen)
{
    uint32_t Number1, Number2;
    uint8_t i;
    if (Number >= 0)
    {
        OLED_ShowChar(Line, Column, '+');
        Number1 = Number;
        Number2 = (Number - Number1) * 1000;
    }
    else
    {
        OLED_ShowChar(Line, Column, '-');
        Number = -Number;
        Number1 = Number;
        Number2 = (Number - Number1) * 1000;
    }
    for (i = 0; i < IntLen; i++)
    {
        OLED_ShowChar(Line, Column + IntLen - i, Number1 % 10 + '0');
        Number1 /= 10;
    }
    OLED_ShowChar(Line, Column + IntLen + 1, '.');
    for (i = 0; i < FraLen; i++)
    {
        OLED_ShowChar(Line, Column + IntLen + 2 + FraLen - i - 1, Number2 % 10 + '0');
        Number2 /= 10;
    }
}

/* 批量写入显示数据：软件I2C版本 */
void OLED_WriteDataBuffer(const uint8_t *Data, uint16_t Len)
{
    uint16_t i;
    if (Len > 128) Len = 128;

    MyI2C_Start();
    MyI2C_SendByte(0x78);
    MyI2C_ReceiveAck();
    MyI2C_SendByte(0x40);
    MyI2C_ReceiveAck();

    for (i = 0; i < Len; i++)
    {
        MyI2C_SendByte(Data[i]);
        MyI2C_ReceiveAck();
    }

    MyI2C_Stop();
}

/**
 * @brief  在指定行显示带像素偏移的字符串（滚动字幕核心函数）
 * @param  Line    行号 1~4
 * @param  String  滚动内容（建议前后补空格，实现从右侧进入、左侧退出）
 * @param  Offset  像素偏移量，每帧 +1 即向左平滑滚动 1 像素
 * @retval None
 */
void OLED_ShowStringShift(uint8_t Line, const char *String, uint16_t Offset)
{
    uint16_t len = strlen(String);
    uint8_t buf[128];
    uint16_t x;
    uint8_t page;

    for (page = 0; page < 2; page++)
    {
        for (x = 0; x < 128; x++)
        {
            uint16_t v = x + Offset;              /* 屏幕列 x 对应虚拟列 v */
            char c = (v / 8 < len) ? String[v / 8] : ' ';
            if (c < ' ' || c > '~') c = ' ';      /* 字库范围保护 */
            buf[x] = OLED_F8x16[c - ' '][page * 8 + v % 8];
        }
        OLED_SetCursor((Line - 1) * 2 + page, 0);
        OLED_WriteDataBuffer(buf, 128);           /* 整页一次写入 */
    }
}

void OLED_Init(void)
{
    MyI2C_Init();  // 初始化软件I2C
    HAL_Delay(200);

    OLED_WriteCommand(0xAE);  // Display OFF
    HAL_Delay(10);

    OLED_WriteCommand(0xD5);  // Set display clock divide ratio/oscillator frequency
    OLED_WriteCommand(0x80);
    HAL_Delay(10);

    OLED_WriteCommand(0xA8);  // Set multiplex ratio
    OLED_WriteCommand(0x3F);
    HAL_Delay(10);

    OLED_WriteCommand(0xD3);  // Set display offset
    OLED_WriteCommand(0x00);

    OLED_WriteCommand(0x40);  // Set display start line

    OLED_WriteCommand(0x8D);  // Charge pump setting (重要！)
    OLED_WriteCommand(0x14);  // 0x14=Enable charge pump
    HAL_Delay(10);

    OLED_WriteCommand(0xA1);  // Set segment re-map (0xA0/0xA1)
    OLED_WriteCommand(0xC8);  // Set COM output scan direction (0xC0/0xC8)

    OLED_WriteCommand(0xDA);  // Set COM pins hardware configuration
    OLED_WriteCommand(0x12);

    OLED_WriteCommand(0x81);  // Set contrast control
    OLED_WriteCommand(0xFF);  // 最大对比度
    HAL_Delay(10);

    OLED_WriteCommand(0xD9);  // Set pre-charge period
    OLED_WriteCommand(0xF1);

    OLED_WriteCommand(0xDB);  // Set VCOMH deselect level
    OLED_WriteCommand(0x40);

    OLED_WriteCommand(0xA4);  // Entire display ON (resume to RAM content)
    OLED_WriteCommand(0xA6);  // Set normal display (not inverse)

    HAL_Delay(100);
    OLED_WriteCommand(0xAF);  // Display ON
    HAL_Delay(100);

    OLED_Clear();
}

/* ==================== 图形绘制功能 ==================== */

/* 显示缓存：128x64像素 = 8页 x 128列 */
static uint8_t OLED_DisplayBuf[8][128];

/* 清空显示缓存 */
void OLED_ClearBuffer(void)
{
    uint8_t i, j;
    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 128; j++)
        {
            OLED_DisplayBuf[i][j] = 0x00;
        }
    }
}

/* 更新显示（将缓存写入OLED） */
void OLED_UpdateDisplay(void)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        OLED_SetCursor(i, 0);
        OLED_WriteDataBuffer(OLED_DisplayBuf[i], 128);
    }
}

/* 设置单个像素 */
void OLED_SetPixel(uint8_t X, uint8_t Y, uint8_t Value)
{
    uint8_t page, bit;
    if (X >= 128 || Y >= 64) return;

    page = Y / 8;
    bit = Y % 8;

    if (Value)
        OLED_DisplayBuf[page][X] |= (1 << bit);
    else
        OLED_DisplayBuf[page][X] &= ~(1 << bit);
}

/* 画线（Bresenham算法） */
void OLED_DrawLine(uint8_t X0, uint8_t Y0, uint8_t X1, uint8_t Y1)
{
    int16_t dx = X1 > X0 ? X1 - X0 : X0 - X1;
    int16_t dy = Y1 > Y0 ? Y1 - Y0 : Y0 - Y1;
    int16_t sx = X0 < X1 ? 1 : -1;
    int16_t sy = Y0 < Y1 ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    while (1)
    {
        OLED_SetPixel(X0, Y0, 1);
        if (X0 == X1 && Y0 == Y1) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; X0 += sx; }
        if (e2 < dx) { err += dx; Y0 += sy; }
    }
}

/* 画圆（Bresenham圆算法） */
void OLED_DrawCircle(uint8_t X0, uint8_t Y0, uint8_t Radius)
{
    int16_t x = 0;
    int16_t y = Radius;
    int16_t d = 3 - 2 * Radius;

    while (x <= y)
    {
        OLED_SetPixel(X0 + x, Y0 + y, 1);
        OLED_SetPixel(X0 - x, Y0 + y, 1);
        OLED_SetPixel(X0 + x, Y0 - y, 1);
        OLED_SetPixel(X0 - x, Y0 - y, 1);
        OLED_SetPixel(X0 + y, Y0 + x, 1);
        OLED_SetPixel(X0 - y, Y0 + x, 1);
        OLED_SetPixel(X0 + y, Y0 - x, 1);
        OLED_SetPixel(X0 - y, Y0 - x, 1);

        if (d < 0)
            d = d + 4 * x + 6;
        else
        {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/* 画实心圆 */
void OLED_FillCircle(uint8_t X0, uint8_t Y0, uint8_t Radius)
{
    int16_t x, y;
    for (y = -Radius; y <= Radius; y++)
    {
        for (x = -Radius; x <= Radius; x++)
        {
            if (x * x + y * y <= Radius * Radius)
            {
                OLED_SetPixel(X0 + x, Y0 + y, 1);
            }
        }
    }
}

/* 在缓存中显示字符（8x16字体） */
void OLED_ShowCharInBuffer(uint8_t X, uint8_t Y, char Char)
{
    uint8_t i;
    if (Char < ' ' || Char > '~') Char = ' ';

    /* 字符占2页，每页8列 */
    for (i = 0; i < 8; i++)
    {
        if (X + i < 128 && Y / 8 < 8)
        {
            OLED_DisplayBuf[Y / 8][X + i] = OLED_F8x16[Char - ' '][i];
        }
        if (X + i < 128 && Y / 8 + 1 < 8)
        {
            OLED_DisplayBuf[Y / 8 + 1][X + i] = OLED_F8x16[Char - ' '][i + 8];
        }
    }
}

/* 在缓存中显示字符串 */
void OLED_ShowStringInBuffer(uint8_t X, uint8_t Y, const char *String)
{
    uint8_t i = 0;
    while (String[i] != '\0')
    {
        OLED_ShowCharInBuffer(X + i * 8, Y, String[i]);
        i++;
    }
}

