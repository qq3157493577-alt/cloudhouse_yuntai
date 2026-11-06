#ifndef __SERIAL_H
#define __SERIAL_H
#include <stdint.h>
#define SERIAL_LINE_MAX 32
void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendArry(uint8_t *Array, uint16_t Count);
void Serial_SendString(const char *String);
void Serial_SendNumber(uint32_t Number, uint8_t Digits);
void Serial_Printf(const char *Format, ...);
uint16_t Serial_TxFree(void);
uint8_t Serial_GetLine(char Line[SERIAL_LINE_MAX + 1]);
uint8_t Serial_GetRxFlag(void);
uint8_t Serial_GetRxData(void);
#endif
