#ifndef __LIGHT_TRACK_H
#define __LIGHT_TRACK_H

#include <stdint.h>
#include "LightSensor.h"

// 0：越亮 ADC 越小；1：越亮 ADC 越大。
#ifndef LIGHT_ADC_BRIGHT_HIGH
#define LIGHT_ADC_BRIGHT_HIGH  0
#endif

// 若实际朝较暗处转，只反转对应轴的正负号，不要改模式 2 的方向。
#ifndef LIGHT_PITCH_UP_DIRECTION
#define LIGHT_PITCH_UP_DIRECTION  1.0f // PA8：光敏板整体翻转后，反转寻光上下方向。
#endif
#ifndef LIGHT_YAW_RIGHT_DIRECTION
#define LIGHT_YAW_RIGHT_DIRECTION  -1.0f // PA11：光敏板整体翻转后，反转寻光左右方向。
#endif

typedef struct {
    uint16_t Raw[LIGHT_COUNT];
    uint16_t Left, Right, Up, Down;  // 转为越亮越大的亮度指标后取平均。
    float PitchTarget, YawTarget;
    uint8_t Valid;
} LightTrackData;

void LightTrack_Init(void);
uint8_t LightTrack_CheckHealth(void);
void LightTrack_Begin(uint32_t Now);
// 在主循环的模式 3 调用，内部每 20 ms 更新一次。
void LightTrack_Update(uint32_t Now);
const LightTrackData *LightTrack_GetData(void);
const char *LightTrack_GetStatus(void);

#endif
