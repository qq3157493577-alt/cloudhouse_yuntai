#include "stm32f10x.h"
#include "LED.h"
#include "Key.h"
#include "OLED.h"
#include "Timer.h"
#include "Servo.h"
#include "MPU6050.h"
#include "Serial.h"
#include "LightTrack.h"
#include "App.h"


#define MPU_RETRY_INTERVAL_MS 3000u

int16_t AX, AY, AZ, GX, GY, GZ;
float Pitch = 0, Roll = 0, Yaw = 0;
uint8_t DMPStatus, DMPHealthy;

int main(void)
{
    uint32_t Now, LastReadMs, LastDataMs, LastRetryMs;
    uint8_t ReadResult, HasData = 0, ReadFault = 0;
    AppAttitude Attitude;
    LED_Init();
    Key_Init();
    Servo_Init();
    Servo1_SetAngle(90);
    Servo2_SetAngle(90);
    Timer_Init();
    OLED_Init();
    LightTrack_Init();
    OLED_ShowString(0, 0, "CLOUDHOUSE", OLED_6X8);
    OLED_ShowString(0, 8, "MODE:1 KEY:-", OLED_6X8);
    OLED_ShowString(0, 16, "CMD:-", OLED_6X8);
    OLED_ShowString(0, 32, "PITCH:090.0 YAW:090.0", OLED_6X8);
    OLED_ShowString(0, 48, "CAL:STARTUP", OLED_6X8);
    OLED_ShowString(0, 56, "KEEP LEVEL STILL", OLED_6X8);
    
    while (OLED_IsBusy())
    {
        OLED_UpdateStep();
    }
    LED_check();
    DMPStatus = MPU6050_DMPInit();
    Serial_Init(); 
    Now = Timer_GetMillis();
    LastReadMs = Now;
    LastDataMs = Now;
    LastRetryMs = Now;
    App_Init(Now);
    while (1)
    {
        Attitude.Fresh = 0;
        Now = Timer_GetMillis();
        if (DMPStatus == 0 && (uint32_t)(Now - LastReadMs) >= 5u)
        {
            LastReadMs = Now;
            ReadResult = MPU6050_ReadDMP(&Pitch, &Roll, &Yaw);
            if (ReadResult == 0)
            {
                HasData = 1;
                ReadFault = 0;
                Attitude.Fresh = 1;
                LastDataMs = Timer_GetMillis();
                MPU6050_GetDMPData(&AX, &AY, &AZ, &GX, &GY, &GZ);
            }
            else if (ReadResult == 2)
            {
                ReadFault = 1;
            }
        }
        Now = Timer_GetMillis();
        DMPHealthy = DMPStatus == 0 && HasData && !ReadFault &&
                     (uint32_t)(Now - LastDataMs) < 500u;
        if (!DMPHealthy &&
            (uint32_t)(Now - LastDataMs) >= MPU_RETRY_INTERVAL_MS &&
            (uint32_t)(Now - LastRetryMs) >= MPU_RETRY_INTERVAL_MS)
        {
            
            HasData = 0;
            ReadFault = 0;
            Attitude.Fresh = 0;
            DMPStatus = MPU6050_DMPInit();
            Now = Timer_GetMillis();
            LastRetryMs = Now;  
            LastReadMs = Now;
            LastDataMs = Now;
            DMPHealthy = 0;  
        }
        Attitude.Pitch = Pitch;
        Attitude.Roll = Roll;
        Attitude.GX = GX;
        Attitude.GY = GY;
        Attitude.GZ = GZ;
        Attitude.Healthy = DMPHealthy;
        Attitude.InitError = DMPStatus;
        App_Run(Now, &Attitude);
    }
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        Timer_Tick1ms();
        Key_Scan1ms();
    }
}
