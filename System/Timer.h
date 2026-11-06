#ifndef __TIMER_H
#define __TIMER_H

#include <stdint.h>
void Timer_Init(void);
void Timer_Tick1ms(void);
uint32_t Timer_GetMillis(void);

#endif
