#include "App.h"
#include "Key.h"
#include "Serial.h"
#include "Servo.h"
#include "LED.h"
#include "OLED.h"
#include "LightTrack.h"
#include <stdio.h>
#include <string.h>

#define CAL_TIMEOUT_MS 10000u
#define CAL_STILL_MS 4000u
#define CAL_LEVEL_TOLERANCE_DEG 5.0f
#define CAL_GYRO_STILL_LIMIT 49
enum { CAL_NONE = 0, CAL_MANUAL = 1, CAL_STARTUP = 2 };
static uint8_t Mode, LastKey, ADCHealthy, LastFault;
static uint8_t CalPending, CalFailed, OLEDErrorReported;
static uint32_t ModeStartMs, FaultStartMs, CalStartMs, LastDisplayMs;
static uint32_t LastADCCheckMs, LastOLEDRetryMs;
static uint32_t RateStartMs;
static uint32_t CalStillStartMs, CalLastFreshMs;
static uint16_t Samples, AttitudeHz;
static char LastCommand[SERIAL_LINE_MAX + 1];


static uint8_t App_CalLevelValid(const AppAttitude *A)
{
    float Pitch, Roll;
    if (Servo_HasReference())
    {
        Pitch = Servo_GetRelativePitch(A->Pitch);
        Roll = Servo_GetRelativeRoll(A->Roll);
    }
    else
    {
        Pitch = A->Pitch;
        Roll = A->Roll;
    }
    return A->Healthy &&
           !A->InitError &&
           Pitch >= -CAL_LEVEL_TOLERANCE_DEG && Pitch <= CAL_LEVEL_TOLERANCE_DEG &&
           Roll >= -CAL_LEVEL_TOLERANCE_DEG && Roll <= CAL_LEVEL_TOLERANCE_DEG;
}


static uint8_t App_CalPoseValid(const AppAttitude *A)
{
    return App_CalLevelValid(A) &&
           A->GX >= -CAL_GYRO_STILL_LIMIT && A->GX <= CAL_GYRO_STILL_LIMIT && A->GY >= -CAL_GYRO_STILL_LIMIT && A->GY <= CAL_GYRO_STILL_LIMIT &&
           A->GZ >= -CAL_GYRO_STILL_LIMIT && A->GZ <= CAL_GYRO_STILL_LIMIT;
}

static void App_EnterMode(uint8_t Next, uint32_t Now, const AppAttitude *A)
{
    if (CalPending == CAL_MANUAL)
    {
        Serial_SendString("ERR CAL\r\n");
        CalPending = CAL_NONE;
    }
    
    if (CalPending == CAL_NONE)
    {
        Servo_FollowCancel();
    }
    Mode = Next;
    ModeStartMs = Now;
    if (Mode == 1)
    {
        Servo1_SetAngle(90.0f);
        Servo2_SetAngle(90.0f);
        LED2_OFF();
    }
    else if (Mode == 2)
    {
        if (CalPending == CAL_NONE)
        {
            Servo_FollowUseReference(Now);
            Servo_FollowUpdate(A->Pitch, A->Roll, Now, 1, A->Healthy);
        }
        LED2_ON();
    }
    else
    {
        LightTrack_Begin(Now);
        LED2_ON();
    }
}

void App_Init(uint32_t Now)
{
    Mode = 1;
    LastKey = 0;
    CalPending = CAL_NONE;
    CalFailed = 0;
    OLEDErrorReported = 0;
    Samples = 0;
    AttitudeHz = 0;
    ADCHealthy = LightTrack_CheckHealth();
    LastFault = 0;
    ModeStartMs = Now;
    FaultStartMs = Now;
    LastADCCheckMs = Now;
    LastOLEDRetryMs = Now;
    LastDisplayMs = Now - 100u;
    RateStartMs = Now;
    strcpy(LastCommand, "-");
    LED2_OFF();
    Servo1_SetAngle(90.0f);
    Servo2_SetAngle(90.0f);
    Servo_CalibrationBegin(Now);
    CalPending = CAL_STARTUP; 
    CalStartMs = Now;
    CalStillStartMs = Now;
    CalLastFreshMs = Now;
}

uint8_t App_GetMode(void)
{
    return Mode;
}


static uint8_t App_ParseAngle(const char *Text, uint16_t *Angle)
{
    uint32_t Value = 0;
    uint8_t Negative = 0;
    if (*Text == '+' || *Text == '-')
    {
        if (*Text == '-')
        {
            Negative = 1;
        }
        Text++;
    }
    if (*Text == '\0')
    {
        return 0;
    }
    while (*Text)
    {
        if (*Text < '0' || *Text > '9')
        {
            return 0;
        }
        Value = Value * 10u + (uint8_t)(*Text - '0');
        Text++;
        if (Value > 180u)
        {
            return 0; 
        }
    }
    if (Negative && Value != 0)
    {
        return 0;
    }
    *Angle = (uint16_t)Value;
    return 1;
}

static void App_Command(char *Line, uint8_t FrameResult, uint32_t Now,
                        const AppAttitude *A)
{
    uint8_t i, IsPitch;
    uint16_t Angle;
    char *Argument;
    if (FrameResult != 1)
    {
        Serial_SendString("ERR\r\n");
        return;
    }
    for (i = 0; Line[i]; i++)
    {
        if (Line[i] >= 'a' && Line[i] <= 'z')
        {
            Line[i] -= 'a' - 'A';
        }
    }
    if (strcmp(Line, "AT") == 0)
    {
        strcpy(LastCommand, "AT");
        Serial_SendString("OK\r\n");
    }
    else if (strcmp(Line, "STATUS") == 0)
    {
        strcpy(LastCommand, "STATUS");
        Serial_Printf("OK MODE=%u\r\n", Mode);
    }
    else if (strcmp(Line, "CAL") == 0)
    {
        if (CalPending == CAL_MANUAL || !App_CalLevelValid(A))
        {
            Serial_SendString("ERR CAL\r\n");
            return;
        }
        Servo_CalibrationBegin(Now);
        CalPending = CAL_MANUAL;
        CalStartMs = Now;
        CalStillStartMs = Now;
        CalLastFreshMs = Now;
        CalFailed = 0;
        strcpy(LastCommand, Line);
    }
    else
    {
        if (strncmp(Line, "PITCH ", 6) == 0)
        {
            IsPitch = 1;
            Argument = Line + 6;
        }
        else if (strncmp(Line, "YAW ", 4) == 0)
        {
            IsPitch = 0;
            Argument = Line + 4;
        }
        else
        {
            Serial_SendString("ERR\r\n");
            return;
        }
        if (Mode != 1 || !App_ParseAngle(Argument, &Angle))
        {
            Serial_SendString("ERR\r\n");
            return;
        }
        if (IsPitch)
        {
            Servo1_SetAngle((float)Angle);
        }
        else
        {
            Servo2_SetAngle((float)Angle);
        }
        strcpy(LastCommand, Line);
        Serial_SendString("OK\r\n");
    }
}

static void App_Display(uint32_t Now, const AppAttitude *A)
{
    const LightTrackData *L = LightTrack_GetData();
    const char *Status;
    const char *KeyText;
    if (!OLED_IsHealthy() || OLED_IsBusy() || (uint32_t)(Now - LastDisplayMs) < 100u)
    {
        return;
    }
    LastDisplayMs = Now;
    OLED_Clear();
    OLED_ShowString(0, 0, "CLOUDHOUSE", OLED_6X8);
    if (LastKey == 1)
    {
        KeyText = "SHORT";
    }
    else if (LastKey == 2)
    {
        KeyText = "LONG";
    }
    else
    {
        KeyText = "-";
    }
    OLED_Printf(0, 8, OLED_6X8, "MODE:%u KEY:%s", Mode, KeyText);
    
    OLED_Printf(0, 16, OLED_6X8, "CMD:%.17s", LastCommand);
    if (strlen(LastCommand) > 17)
    {
        OLED_Printf(0, 24, OLED_6X8, "    %s", LastCommand + 17);
    }
    OLED_Printf(0, 32, OLED_6X8, "PITCH:%05.1f YAW:%05.1f", Servo1_GetAngle(), Servo2_GetAngle());
    if (Mode == 3)
    {
        if (L->Valid)
        {
            
            OLED_Printf(0, 40, OLED_6X8, "UL:%4u UR:%4u", L->Raw[LIGHT_DR], L->Raw[LIGHT_DL]);
            OLED_Printf(0, 48, OLED_6X8, "DL:%4u DR:%4u", L->Raw[LIGHT_UR], L->Raw[LIGHT_UL]);
        }
        else
        {
            OLED_ShowString(0, 40, "UL:---- UR:----", OLED_6X8);
            OLED_ShowString(0, 48, "DL:---- DR:----", OLED_6X8);
        }
    }
    else
    {
        if (A->Healthy && Servo_HasReference())
        {
            OLED_Printf(0, 40, OLED_6X8, "P:%+05.1f R:%+05.1f",
                        Servo_GetRelativePitch(A->Pitch), Servo_GetRelativeRoll(A->Roll));
        }
        else if (A->Healthy)
        {
            OLED_Printf(0, 40, OLED_6X8, "RAW P:%+05.1f R:%+05.1f", A->Pitch, A->Roll);
        }
        else
        {
            OLED_ShowString(0, 40, "P:---- R:----", OLED_6X8);
        }
        if (CalPending != CAL_NONE && !App_CalPoseValid(A))
        {
            OLED_Printf(0, 48, OLED_6X8, "CAL:WAIT HZ:%u", AttitudeHz);
        }
        else if (CalPending != CAL_NONE && (uint32_t)(Now - CalStillStartMs) < CAL_STILL_MS)
        {
            OLED_Printf(0, 48, OLED_6X8, "CAL:%lu/%lus HZ:%u",
                        (unsigned long)((uint32_t)(Now - CalStillStartMs) / 1000u),
                        (unsigned long)(CAL_STILL_MS / 1000u), AttitudeHz);
        }
        else if (CalPending != CAL_NONE)
        {
            OLED_Printf(0, 48, OLED_6X8, "CAL:%02u/25 HZ:%u", Servo_FollowGetCalibrationCount(), AttitudeHz);
        }
        else
        {
            if (CalFailed)
            {
                Status = "FAIL";
            }
            else if (Servo_HasReference())
            {
                Status = "READY";
            }
            else
            {
                Status = "NOT SET";
            }
            OLED_Printf(0, 48, OLED_6X8, "CAL:%s HZ:%u", Status, AttitudeHz);
        }
    }
    if (A->InitError)
    {
        OLED_Printf(0, 56, OLED_6X8, "ERR:MPU INIT E%02u", A->InitError);
    }
    else
    {
        if (!A->Healthy)
        {
            Status = "ERR:MPU DATA";
        }
        else if (!ADCHealthy)
        {
            Status = "ERR:ADC";
        }
        else if (CalFailed)
        {
            Status = "ERR:CAL SEND CAL";
        }
        else if (CalPending != CAL_NONE)
        {
            if (App_CalPoseValid(A))
            {
                Status = "KEEP LEVEL STILL";
            }
            else
            {
                Status = "PLACE LEVEL + STILL";
            }
        }
        else if (!Servo_HasReference())
        {
            Status = "NEED CAL LEVEL STILL";
        }
        else if (Mode == 3)
        {
            Status = LightTrack_GetStatus();
        }
        else
        {
            Status = "RUN:OK";
        }
        OLED_ShowString(0, 56, (char *)Status, OLED_6X8);
    }
    OLED_BeginUpdate();
}

void App_Run(uint32_t Now, const AppAttitude *A)
{
    uint8_t Key, Next, Result, Budget = 8, Fault, CalPoseValid;
    char Line[SERIAL_LINE_MAX + 1];
    uint32_t Elapsed;
    
    if (Serial_TxFree() >= 96u)
    {
        Key = Key_GetNum();
        if (Key == 1 || Key == 2)
        {
            LastKey = Key;
            if (Key == 1)
            {
                Serial_SendString("KEY S\r\n");
                if (Mode == 3)
                {
                    Next = 1;
                }
                else
                {
                    Next = Mode + 1;
                }
            }
            else
            {
                Serial_SendString("KEY L\r\n");
                if (Mode == 1)
                {
                    Next = 3;
                }
                else
                {
                    Next = Mode - 1;
                }
            }
            App_EnterMode(Next, Now, A);
            Serial_Printf("MODE %u\r\n", Mode);
        }
    }
    while (Budget > 0 && Serial_TxFree() >= 96u)
    {
        Budget--;
        Result = Serial_GetLine(Line);
        if (!Result)
        {
            break;
        }
        App_Command(Line, Result, Now, A);
    }
    CalPoseValid = App_CalPoseValid(A);
    if (CalPending != CAL_NONE)
    {
        
        if (!CalPoseValid || (uint32_t)(Now - CalLastFreshMs) > 100u)
        {
            CalStillStartMs = Now;
            Servo_CalibrationBegin(Now);
        }
        if (A->Fresh && CalPoseValid)
        {
            CalLastFreshMs = Now;
        }
        if ((uint32_t)(Now - CalStillStartMs) >= CAL_STILL_MS)
        {
            Servo_FollowUpdate(A->Pitch, A->Roll, Now, A->Fresh, CalPoseValid);
        }
        if (Servo_FollowIsReady())
        {
            if (CalPending == CAL_MANUAL)
            {
                Serial_SendString("OK\r\n");
            }
            CalPending = CAL_NONE;
            CalFailed = 0;
            if (Mode == 2)
            {
                Servo_FollowUpdate(A->Pitch, A->Roll, Now, A->Fresh, A->Healthy);
            }
            else
            {
                Servo_FollowCancel();
            }
        }
        
        else if (CalPending == CAL_MANUAL && (!A->Healthy || A->InitError ||
                 (uint32_t)(Now - CalStartMs) >= CAL_TIMEOUT_MS))
        {
            Serial_SendString("ERR CAL\r\n");
            CalPending = CAL_NONE;
            CalFailed = 1;
            Servo_FollowCancel();
            if (Mode == 2 && Servo_HasReference())
            {
                Servo_FollowUseReference(Now);
            }
        }
    }
    else if (Mode == 2)
    {
        Servo_FollowUpdate(A->Pitch, A->Roll, Now, A->Fresh, A->Healthy);
        if (Servo_FollowIsReady())
        {
            CalFailed = 0;
        }
    }
    if (Mode == 3)
    {
        LightTrack_Update(Now);
    }

    if (Mode == 3)
    {
        ADCHealthy = LightTrack_GetData()->Valid;
    }
    if ((uint32_t)(Now - LastADCCheckMs) >= 250u)
    {
        LastADCCheckMs = Now;
        if (Mode != 3)
        {
            ADCHealthy = LightTrack_CheckHealth();
        }
        else if (!ADCHealthy)
        {
            (void)LightTrack_CheckHealth();
        }
    }
    if (!OLED_IsHealthy())
    {
        if (!OLEDErrorReported && Serial_TxFree() >= 32u)
        {
            Serial_SendString("ERR OLED\r\n");
            OLEDErrorReported = 1;
        }
        if ((uint32_t)(Now - LastOLEDRetryMs) >= 1000u)
        {
            LastOLEDRetryMs = Now;
            OLED_Init(); 
        }
    }
    else
    {
        OLEDErrorReported = 0;
    }

    Fault = !A->Healthy || !ADCHealthy || !OLED_IsHealthy() || CalFailed ||
            (Mode == 2 && CalPending == CAL_NONE && !Servo_HasReference());
    if (Fault != LastFault)
    {
        FaultStartMs = Now;
        LastFault = Fault;
    }
    
    if (!Fault || (((uint32_t)(Now - FaultStartMs) / 500u) % 2u) == 0)
    {
        LED1_ON();
    }
    else
    {
        LED1_OFF();
    }
    if (Mode == 1)
    {
        LED2_OFF();
    }
    else if (Mode == 2 || (((uint32_t)(Now - ModeStartMs) / 500u) % 2u) == 0)
    {
        LED2_ON();
    }
    else
    {
        LED2_OFF();
    }

    if (A->Fresh && Samples < 65535u)
    {
        Samples++;
    }
    Elapsed = (uint32_t)(Now - RateStartMs);
    if (Elapsed >= 1000u)
    {
        AttitudeHz = (uint16_t)((uint32_t)Samples * 1000u / Elapsed);
        Samples = 0;
        RateStartMs = Now;
    }
    App_Display(Now, A);
    OLED_UpdateStep();
}
