#include "stm32f10x.h"
#include "LightSensor.h"
#include "Timer.h"

#define ADC_TIMEOUT_MS  3u
#define ADC_AVERAGES    4u

// A1 左下 PA7，A2 左上 PA6，A3 右上 PA5，A4 右下 PA4。
static const uint8_t Channels[LIGHT_COUNT] = {
    ADC_Channel_7, ADC_Channel_6, ADC_Channel_5, ADC_Channel_4
};
static uint8_t ADCReady;

uint8_t LightSensor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;
    uint32_t Start;

    ADCReady = 0;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);  // 当前 PCLK2=72 MHz，ADC=12 MHz。
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_StructInit(&ADC_InitStructure);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    ADC_Cmd(ADC1, ENABLE);

    // 仅上电初始化等待 1 ms，之后每次采样约几十微秒。
    Start = Timer_GetMillis();
    while ((uint32_t)(Timer_GetMillis() - Start) < 1u) {}
    ADC_ResetCalibration(ADC1);
    Start = Timer_GetMillis();
    while (ADC_GetResetCalibrationStatus(ADC1) == SET)
    {
        if ((uint32_t)(Timer_GetMillis() - Start) >= ADC_TIMEOUT_MS)
        {
            return 1;
        }
    }
    ADC_StartCalibration(ADC1);
    Start = Timer_GetMillis();
    while (ADC_GetCalibrationStatus(ADC1) == SET)
    {
        if ((uint32_t)(Timer_GetMillis() - Start) >= ADC_TIMEOUT_MS)
        {
            return 1;
        }
    }

    ADCReady = 1;
    return 0;
}

uint8_t LightSensor_Read(uint16_t Raw[LIGHT_COUNT])
{
    uint8_t Position, Sample;
    uint16_t Result[LIGHT_COUNT];
    uint32_t Start, Sum;
    if (!ADCReady || Raw == 0)
    {
        return 1;
    }

    for (Position = 0; Position < LIGHT_COUNT; Position++)
    {
        Sum = 0;
        ADC_RegularChannelConfig(ADC1, Channels[Position], 1, ADC_SampleTime_239Cycles5);
        for (Sample = 0; Sample < ADC_AVERAGES; Sample++)
        {
            ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
            ADC_SoftwareStartConvCmd(ADC1, ENABLE);
            Start = Timer_GetMillis();
            while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)
            {
                if ((uint32_t)(Timer_GetMillis() - Start) >= ADC_TIMEOUT_MS)
                {
                    return 1;
                }
            }
            Sum += ADC_GetConversionValue(ADC1);
        }
        Result[Position] = (uint16_t)(Sum / ADC_AVERAGES);
    }
    
    for (Position = 0; Position < LIGHT_COUNT; Position++)
    {
        Raw[Position] = Result[Position];
    }
    return 0;
}
