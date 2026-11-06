#include "stm32f10x.h"
#include "Serial.h"
#include "LED.h"
#include <stdio.h>
#include <stdarg.h>

// USART1: PA9 TX, PA10 RX, 115200-8-N-1.
#define RX_SIZE 2048u
#define TX_SIZE 2048u
static uint8_t RxBuffer[RX_SIZE], TxBuffer[TX_SIZE];
static volatile uint16_t RxHead, RxTail, TxHead, TxTail;
static volatile uint8_t RxLost;
static char Assembly[SERIAL_LINE_MAX + 1];
static uint8_t Length, BadFrame, SawCR, Discard;

void Serial_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    RxHead = 0;
    RxTail = 0;
    TxHead = 0;
    TxTail = 0;
    RxLost = 0;
    Length = 0;
    BadFrame = 0;
    SawCR = 0;
    Discard = 0;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    USART_StructInit(&USART_InitStructure);
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

uint16_t Serial_TxFree(void)
{
    
    return (uint16_t)((TxTail - TxHead - 1u) & (TX_SIZE - 1u));
}

void Serial_SendByte(uint8_t Byte)
{
    uint16_t Next = (uint16_t)((TxHead + 1u) & (TX_SIZE - 1u));
    
    while (Next == TxTail)
    {
        
    }
    TxBuffer[TxHead] = Byte;
    __DMB();
    TxHead = Next;
    USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
}

void Serial_SendArry(uint8_t *Array, uint16_t Count)
{
    uint16_t i;
    for (i = 0; i < Count; i++)
    {
        Serial_SendByte(Array[i]);
    }
}

void Serial_SendString(const char *String)
{
    while (*String != '\0')
    {
        Serial_SendByte((uint8_t)*String);
        String++;
    }
}

void Serial_SendNumber(uint32_t Number, uint8_t Digits)
{
    char Buffer[16];
    if (Digits > 10)
    {
        Digits = 10;
    }
    snprintf(Buffer, sizeof(Buffer), "%0*lu", Digits, (unsigned long)Number);
    Serial_SendString(Buffer);
}

void Serial_Printf(const char *Format, ...)
{
    char Buffer[100];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Buffer, sizeof(Buffer), Format, Args);
    va_end(Args);
    Serial_SendString(Buffer);
}


uint8_t Serial_GetLine(char Line[SERIAL_LINE_MAX + 1])
{
    uint16_t Budget = 128;
    uint8_t Byte, i, Result;
    uint32_t Mask;
    if (RxLost)
    {
        Mask = __get_PRIMASK();
        __disable_irq();
        RxTail = RxHead;
        RxLost = 0;
        __set_PRIMASK(Mask);
        Discard = 1;
        Length = 0;
        BadFrame = 0;
        SawCR = 0;
    }
    while (Budget > 0 && RxTail != RxHead)
    {
        Budget--;
        Byte = RxBuffer[RxTail];
        RxTail = (uint16_t)((RxTail + 1u) & (RX_SIZE - 1u));
        if (Byte == '\n')
        {
            if (Discard || BadFrame || Length == 0)
            {
                Result = 2;
            }
            else
            {
                Result = 1;
            }
            for (i = 0; i < Length; i++)
            {
                Line[i] = Assembly[i];
            }
            Line[Length] = '\0';
            Length = 0;
            BadFrame = 0;
            SawCR = 0;
            Discard = 0;
            return Result;
        }
        if (Discard)
        {
            continue;
        }
        if (SawCR)
        {
            BadFrame = 1;
        }
        if (Byte == '\r')
        {
            SawCR = 1;
            continue;
        }
        if (Byte < 32 || Byte > 126)
        {
            BadFrame = 1;
        }
        if (Length < SERIAL_LINE_MAX)
        {
            Assembly[Length] = (char)Byte;
            Length++;
        }
        else
        {
            BadFrame = 1;
        }
    }
    return 0;
}


uint8_t Serial_GetRxFlag(void)
{
    return RxTail != RxHead;
}
uint8_t Serial_GetRxData(void)
{
    uint8_t Byte = 0;
    if (RxTail != RxHead)
    {
        Byte = RxBuffer[RxTail];
        RxTail = (uint16_t)((RxTail + 1u) & (RX_SIZE - 1u));
    }
    return Byte;
}

void USART1_IRQHandler(void)
{
    uint32_t Status = USART1->SR;
    uint16_t Next;
    uint8_t Byte;
    
    if (Status & (USART_FLAG_RXNE | USART_FLAG_ORE | USART_FLAG_FE |
                  USART_FLAG_NE | USART_FLAG_PE))
    {
        Byte = (uint8_t)USART_ReceiveData(USART1);
        if (Status & (USART_FLAG_ORE | USART_FLAG_FE | USART_FLAG_NE | USART_FLAG_PE))
            RxLost = 1;
        if (Status & USART_FLAG_RXNE)
        {
            if (Byte == '\n')
            {
                LED3_Turn(); 
            }
            Next = (uint16_t)((RxHead + 1u) & (RX_SIZE - 1u));
            if (Next != RxTail)
            {
                RxBuffer[RxHead] = Byte;
                __DMB();
                RxHead = Next;
            }
            else
            {
                RxLost = 1;
            }
        }
    }
    if ((Status & USART_FLAG_TXE) && (USART1->CR1 & USART_CR1_TXEIE))
    {
        if (TxTail != TxHead)
        {
            USART_SendData(USART1, TxBuffer[TxTail]);
            TxTail = (uint16_t)((TxTail + 1u) & (TX_SIZE - 1u));
        }
        else
        {
            USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
        }
    }
}
