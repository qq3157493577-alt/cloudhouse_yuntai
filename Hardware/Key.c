#include "stm32f10x.h"                  // Device header
#include "Timer.h"
#include "Key.h"

void Key_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStructure); 
}



static uint8_t Pressed = 0;
static uint8_t ReleasePending = 0;
static uint16_t PressDurationMs = 0;  // 最多 1000，长按再久也不溢出。
static uint32_t PressStartMs, ReleaseStartMs;
#define KEY_EVENT_QUEUE_SIZE 16u

static volatile uint8_t EventQueue[KEY_EVENT_QUEUE_SIZE];
static volatile uint8_t EventHead = 0;
static volatile uint8_t EventTail = 0;

// 每 1 ms 扫描一次：只读引脚、计时，不延时，不等松手。
void Key_Scan1ms(void)
{
    uint32_t Now = Timer_GetMillis();
    uint32_t Duration;
    uint8_t Event, NextHead;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0)
    {
        if (!Pressed)
        {
            Pressed = 1;
            PressStartMs = Now;
            PressDurationMs = 0;
        }
        ReleasePending = 0;
        if (PressDurationMs < 1000)
        {
            Duration = (uint32_t)(Now - PressStartMs);
            if (Duration >= 1000)
            {
                PressDurationMs = 1000;
            }
        }
        return;
    }

    if (!Pressed)
    {
        return;
    }
    if (!ReleasePending)
    {
        if (PressDurationMs >= 1000)
        {
            Duration = 1000;
        }
        else
        {
            Duration = (uint32_t)(Now - PressStartMs);
        }
        if (Duration < 50)
        {
            Pressed = 0;  // 不足 50 ms，直接忽略。
            return;
        }
        if (Duration >= 1000)
        {
            PressDurationMs = 1000;
        }
        else
        {
            PressDurationMs = (uint16_t)Duration;
        }
        ReleaseStartMs = Now;
        ReleasePending = 1;
    }
    if ((uint32_t)(Now - ReleaseStartMs) >= 50)
    {
        if (PressDurationMs >= 1000)
        {
            Event = 2;
        }
        else
        {
            Event = 1;
        }
        NextHead = (uint8_t)((EventHead + 1u) % KEY_EVENT_QUEUE_SIZE);
        if (NextHead != EventTail)
        {
            EventQueue[EventHead] = Event;
            EventHead = NextHead;
        }
        Pressed = 0;
        ReleasePending = 0;
        // 只有松手稳定后才产生一次事件，按住期间不产生事件。
    }
}

// 主循环每次取走一个事件，推进队列下标，按产生顺序处理。
uint8_t Key_GetNum(void)
{
    uint8_t Event = 0;
    uint32_t SavedMask = __get_PRIMASK();
    __disable_irq();
    if (EventTail != EventHead)
    {
        Event = EventQueue[EventTail];
        EventTail = (uint8_t)((EventTail + 1u) % KEY_EVENT_QUEUE_SIZE);
    }
    __set_PRIMASK(SavedMask);
    return Event;
}
