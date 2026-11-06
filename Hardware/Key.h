#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>
void Key_Init(void);
void Key_Scan1ms(void);
// 0：无事件，1：短按，2：长按。均在松手后返回。
uint8_t Key_GetNum(void);

#endif
