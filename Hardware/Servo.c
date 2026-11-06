#include "stm32f10x.h"
#include "PWM.h"
#include "Servo.h"

// 安装方向相反时，将对应方向改为 -1.0f。
#define SERVO1_FOLLOW_DIRECTION  1.0f   // PA8 跟随 Pitch
#define SERVO2_FOLLOW_DIRECTION  -1.0f  // PA11 镜像跟随 Roll
#define CALIBRATION_SAMPLES     25
#define CALIBRATION_STABLE_DEG  3.0f
#define SERVO_SETTLE_MS         200
#define CALIBRATION_GAP_MS      100

enum { FOLLOW_STOPPED = 0, FOLLOW_CALIBRATING = 1, FOLLOW_READY = 2 };
static uint8_t FollowState = FOLLOW_STOPPED;
static uint8_t ReferenceValid = 0;
static uint8_t CalibrationCount = 0;
static uint32_t FollowStartMs, LastSampleMs;
static float PitchZero, RollZero;
static float PitchSum, RollOffsetSum, RollReference;
static float PitchMin, PitchMax, RollMin, RollMax;
static float Servo1Angle = 90.0f, Servo2Angle = 90.0f;

static float Servo_LimitAngle(float Angle)
{
    if (Angle < 0.0f)
    {
        Angle = 0.0f;
    }
    if (Angle > 180.0f)
    {
        Angle = 180.0f;
    }
    return Angle;
}

// 处理 Roll 从 +180° 跳到 -180° 的表示变化。
static float Servo_AngleDifference(float Angle, float Reference)
{
    float Difference = Angle - Reference;
    if (Difference > 180.0f)
    {
        Difference -= 360.0f;
    }
    if (Difference < -180.0f)
    {
        Difference += 360.0f;
    }
    return Difference;
}

static void Servo_StartSampleWindow(float Pitch, float Roll, uint32_t Now)
{
    CalibrationCount = 1;
    PitchSum = Pitch;
    RollOffsetSum = 0.0f;
    RollReference = Roll;
    PitchMin = Pitch;
    PitchMax = Pitch;
    RollMin = 0.0f;
    RollMax = 0.0f;
    LastSampleMs = Now;
}

void Servo_Init(void)
{
    PWM_Init();
}

void Servo1_SetAngle(float Angle)
{
    if (Angle != Angle)
    {
        return;  // NaN 不能作为 PWM 参数。
    }
    Angle = Servo_LimitAngle(Angle);
    Servo1Angle = Angle;
    PWM_SetCompare1((uint16_t)(Angle / 180.0f * 2000.0f + 500.0f));
}

void Servo2_SetAngle(float Angle)
{
    if (Angle != Angle)
    {
        return;
    }
    Angle = Servo_LimitAngle(Angle);
    Servo2Angle = Angle;
    PWM_SetCompare4((uint16_t)(Angle / 180.0f * 2000.0f + 500.0f));
}

float Servo1_GetAngle(void)
{
    return Servo1Angle;
}

float Servo2_GetAngle(void)
{
    return Servo2Angle;
}

void Servo_FollowBegin(uint32_t Now)
{
    FollowState = FOLLOW_CALIBRATING;
    CalibrationCount = 0;
    FollowStartMs = Now;
    Servo1_SetAngle(90.0f);
    Servo2_SetAngle(90.0f);
}

void Servo_FollowCancel(void)
{
    FollowState = FOLLOW_STOPPED;
    CalibrationCount = 0;
}

uint8_t Servo_FollowIsReady(void)
{
    return FollowState == FOLLOW_READY;
}

uint8_t Servo_FollowGetCalibrationCount(void)
{
    return CalibrationCount;
}

void Servo_FollowUpdate(float Pitch, float Roll, uint32_t Now,
                        uint8_t NewData, uint8_t Healthy)
{
    float RollOffset, Angle1, Angle2;
    if (FollowState == FOLLOW_STOPPED)
    {
        return;
    }
    if (!Healthy)
    {
        if (FollowState == FOLLOW_CALIBRATING)
        {
            CalibrationCount = 0;
        }
        return;  // 异常时保持当前舵机位置。
    }
    if (FollowState == FOLLOW_CALIBRATING && CalibrationCount != 0 &&
        (uint32_t)(Now - LastSampleMs) > CALIBRATION_GAP_MS)
        CalibrationCount = 0;
    if (!NewData)
    {
        return;  // 同一组旧角度不能重复计入校准。
    }
    if (!(Pitch >= -90.0f && Pitch <= 90.0f &&
          Roll >= -180.0f && Roll <= 180.0f))
    {
        if (FollowState == FOLLOW_CALIBRATING)
        {
            CalibrationCount = 0;
        }
        return;
    }

    if (FollowState == FOLLOW_CALIBRATING)
    {
        // 舵机居中后留 200 ms，再开始约 0.5 秒的角度采样。
        if ((uint32_t)(Now - FollowStartMs) < SERVO_SETTLE_MS)
        {
            return;
        }
        if (CalibrationCount == 0)
        {
            Servo_StartSampleWindow(Pitch, Roll, Now);
            return;
        }
        RollOffset = Servo_AngleDifference(Roll, RollReference);
        if (Pitch < PitchMin)
        {
            PitchMin = Pitch;
        }
        if (Pitch > PitchMax)
        {
            PitchMax = Pitch;
        }
        if (RollOffset < RollMin)
        {
            RollMin = RollOffset;
        }
        if (RollOffset > RollMax)
        {
            RollMax = RollOffset;
        }
        if (PitchMax - PitchMin > CALIBRATION_STABLE_DEG ||
            RollMax - RollMin > CALIBRATION_STABLE_DEG)
        {
            // 还在移动，当前读数作为下一段稳定采样的起点。
            Servo_StartSampleWindow(Pitch, Roll, Now);
            return;
        }
        PitchSum += Pitch;
        RollOffsetSum += RollOffset;
        CalibrationCount++;
        LastSampleMs = Now;
        if (CalibrationCount >= CALIBRATION_SAMPLES)
        {
            PitchZero = PitchSum / CalibrationCount;
            RollZero = RollReference + RollOffsetSum / CalibrationCount;
            if (RollZero > 180.0f)
            {
                RollZero -= 360.0f;
            }
            if (RollZero < -180.0f)
            {
                RollZero += 360.0f;
            }
            ReferenceValid = 1;
            FollowState = FOLLOW_READY;
        }
        return;
    }

    Angle1 = 90.0f + SERVO1_FOLLOW_DIRECTION * (Pitch - PitchZero);
    Angle2 = 90.0f + SERVO2_FOLLOW_DIRECTION * Servo_AngleDifference(Roll, RollZero);
    Servo1_SetAngle(Angle1);
    Servo2_SetAngle(Angle2);
}

void Servo_CalibrationBegin(uint32_t Now)
{
    FollowState = FOLLOW_CALIBRATING;
    CalibrationCount = 0;
    FollowStartMs = Now;
    
}

uint8_t Servo_HasReference(void)
{
    return ReferenceValid;
}

void Servo_FollowUseReference(uint32_t Now)
{
    (void)Now;
    
    if (ReferenceValid)
    {
        FollowState = FOLLOW_READY;
    }
    else
    {
        FollowState = FOLLOW_STOPPED;
    }
    CalibrationCount = 0;
}

float Servo_GetRelativePitch(float Pitch)
{
    return Pitch - PitchZero;
}

float Servo_GetRelativeRoll(float Roll)
{
    return Servo_AngleDifference(Roll, RollZero);
}
