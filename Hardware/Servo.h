#ifndef __SERVO_H
#define __SERVO_H

#include <stdint.h>
void Servo_Init(void);
void Servo1_SetAngle(float Angle);
void Servo2_SetAngle(float Angle);
// 返回最后下发的目标角度，不是舵机位置反馈。
float Servo1_GetAngle(void);
float Servo2_GetAngle(void);

// 校准仅由上电或 CAL 发起；进入模式 2 使用已保存零点。
void Servo_FollowBegin(uint32_t Now);
void Servo_CalibrationBegin(uint32_t Now);
void Servo_FollowUseReference(uint32_t Now);
uint8_t Servo_HasReference(void);
float Servo_GetRelativePitch(float Pitch);
float Servo_GetRelativeRoll(float Roll);
void Servo_FollowCancel(void);

void Servo_FollowUpdate(float Pitch, float Roll, uint32_t Now,
                        uint8_t NewData, uint8_t Healthy);
uint8_t Servo_FollowIsReady(void);
uint8_t Servo_FollowGetCalibrationCount(void);

#endif
