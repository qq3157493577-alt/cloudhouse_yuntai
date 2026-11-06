#include "LightTrack.h"
#include "Servo.h"

#define LIGHT_UPDATE_MS       20u
#define LIGHT_DARK_LEVEL      80u
#define LIGHT_DEADBAND_BASE   25u
#define LIGHT_DEADBAND_PERCENT 3u
#define LIGHT_STEP_DEG        1.0f

static LightTrackData Data;
static uint16_t Filtered[LIGHT_COUNT];
static uint8_t ADCInitFailed, HasFilter;
static uint32_t LastUpdateMs;
static const char *Status = "WAIT";

static float LightTrack_Limit(float Angle)
{
    if (Angle < 0.0f)
    {
        return 0.0f;
    }
    if (Angle > 180.0f)
    {
        return 180.0f;
    }
    return Angle;
}

void LightTrack_Init(void)
{
    ADCInitFailed = LightSensor_Init();
    Data.Valid = 0;
    HasFilter = 0;
    if (ADCInitFailed)
    {
        Status = "ADC-ERR";
    }
    else
    {
        Status = "WAIT";
    }
}

void LightTrack_Begin(uint32_t Now)
{
    // 从上一模式最后的目标继续，不突然跳回 90 度。
    Data.PitchTarget = Servo1_GetAngle();
    Data.YawTarget = Servo2_GetAngle();
    Data.Valid = 0;
    HasFilter = 0;
    LastUpdateMs = Now - LIGHT_UPDATE_MS;
    if (ADCInitFailed)
    {
        Status = "ADC-ERR";
    }
    else
    {
        Status = "WAIT";
    }
}

void LightTrack_Update(uint32_t Now)
{
    uint8_t i, Moving = 0, Limited = 0;
    uint16_t Raw[LIGHT_COUNT], Bright[LIGHT_COUNT];
    uint32_t Level, XThreshold, YThreshold;
    int32_t XError, YError;
    float PitchStep = 0.0f, YawStep = 0.0f, PitchNext, YawNext;

    if ((uint32_t)(Now - LastUpdateMs) < LIGHT_UPDATE_MS)
    {
        return;
    }
    LastUpdateMs = Now;  // 主循环偶尔变慢也不补发一串转动命令。
    if (ADCInitFailed || LightSensor_Read(Raw) != 0)
    {
        Data.Valid = 0;
        HasFilter = 0;
        Status = "ADC-ERR";
        return;
    }

    for (i = 0; i < LIGHT_COUNT; i++)
    {
        if (Raw[i] > 4095u)
        {
            Data.Valid = 0;
            HasFilter = 0;
            Status = "ADC-ERR";
            return;
        }
    }
    for (i = 0; i < LIGHT_COUNT; i++)
    {
        Data.Raw[i] = Raw[i];  // OLED 显示原始 ADC，便于确认接线和极性。
        if (HasFilter)
        {
            Filtered[i] = (uint16_t)((Filtered[i] * 3u + Raw[i] + 2u) / 4u);
        }
        else
        {
            Filtered[i] = Raw[i];
        }
#if LIGHT_ADC_BRIGHT_HIGH
        Bright[i] = Filtered[i];
#else
        Bright[i] = 4095u - Filtered[i];
#endif
    }
    HasFilter = 1;
    Data.Valid = 1;

    
    Data.Left = (Bright[LIGHT_DL] + Bright[LIGHT_UL]) / 2u;
    Data.Right = (Bright[LIGHT_UR] + Bright[LIGHT_DR]) / 2u;
    Data.Up = (Bright[LIGHT_UL] + Bright[LIGHT_UR]) / 2u;
    Data.Down = (Bright[LIGHT_DL] + Bright[LIGHT_DR]) / 2u;
    Level = (Data.Left + Data.Right) / 2u;
    if (Level < LIGHT_DARK_LEVEL)
    {
        Status = "NO-LIGHT";
        return;
    }

    XError = (int32_t)Data.Right - Data.Left;  // 正数：右边更亮。
    YError = (int32_t)Data.Up - Data.Down;     // 正数：上边更亮。
    XThreshold = LIGHT_DEADBAND_BASE + (Data.Left + Data.Right) * LIGHT_DEADBAND_PERCENT / 100u;
    YThreshold = LIGHT_DEADBAND_BASE + (Data.Up + Data.Down) * LIGHT_DEADBAND_PERCENT / 100u;

    if (XError > (int32_t)XThreshold)
    {
        YawStep = LIGHT_STEP_DEG;
    }
    else if (XError < -(int32_t)XThreshold)
    {
        YawStep = -LIGHT_STEP_DEG;
    }
    if (YError > (int32_t)YThreshold)
    {
        PitchStep = LIGHT_STEP_DEG;
    }
    else if (YError < -(int32_t)YThreshold)
    {
        PitchStep = -LIGHT_STEP_DEG;
    }
    // 接近对准时减小步幅，避免越过中心后反复摆动。
    if (XError < (int32_t)(2u * XThreshold) && XError > -(int32_t)(2u * XThreshold))
    {
        YawStep *= 0.5f;
    }
    if (YError < (int32_t)(2u * YThreshold) && YError > -(int32_t)(2u * YThreshold))
    {
        PitchStep *= 0.5f;
    }

    PitchNext = Data.PitchTarget + LIGHT_PITCH_UP_DIRECTION * PitchStep;
    YawNext = Data.YawTarget + LIGHT_YAW_RIGHT_DIRECTION * YawStep;
    Limited = PitchNext < 0.0f || PitchNext > 180.0f || YawNext < 0.0f || YawNext > 180.0f;
    PitchNext = LightTrack_Limit(PitchNext);
    YawNext = LightTrack_Limit(YawNext);
    if (PitchNext != Data.PitchTarget)
    {
        Data.PitchTarget = PitchNext;
        Servo1_SetAngle(PitchNext);
        Moving = 1;
    }
    if (YawNext != Data.YawTarget)
    {
        Data.YawTarget = YawNext;
        Servo2_SetAngle(YawNext);
        Moving = 1;
    }
    if (Limited)
    {
        Status = "LIMIT";
    }
    else if (Moving)
    {
        Status = "TRACK";
    }
    else
    {
        Status = "HOLD";
    }
}

const LightTrackData *LightTrack_GetData(void)
{
    return &Data;
}

const char *LightTrack_GetStatus(void)
{
    return Status;
}

uint8_t LightTrack_CheckHealth(void)
{
    uint16_t Raw[LIGHT_COUNT];
    if (ADCInitFailed)
    {
        ADCInitFailed = LightSensor_Init();
    }
    return !ADCInitFailed && LightSensor_Read(Raw) == 0;
}
