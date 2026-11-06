#ifndef __LIGHT_SENSOR_H
#define __LIGHT_SENSOR_H

#include <stdint.h>

// 数组顺序按位置：左下、左上、右上、右下。
enum { LIGHT_DL = 0, LIGHT_UL, LIGHT_UR, LIGHT_DR, LIGHT_COUNT };

// 0 成功，1 超时；Init 必须在 Timer_Init 之后调用。
uint8_t LightSensor_Init(void);
uint8_t LightSensor_Read(uint16_t Raw[LIGHT_COUNT]);

#endif
