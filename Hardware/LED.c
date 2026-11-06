#include "stm32f10x.h"
#include "Delay.h"                  // Device header

void LED_Init(void)					//LED初始化
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure); 
	GPIO_SetBits(GPIOA,GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3);
}
void LED1_ON(void)					//开LED1
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_1);
}
void LED1_OFF(void)					//关闭LED1
{
	GPIO_SetBits(GPIOA,GPIO_Pin_1);
}
void LED1_Turn(void)					//切换LED1状态
{
	if(GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_1)==0)
	{
		GPIO_SetBits(GPIOA,GPIO_Pin_1);
	}else{
		GPIO_ResetBits(GPIOA,GPIO_Pin_1);
	}
}
void LED2_ON(void)					//开LED2
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_2);
}
void LED2_OFF(void)					//关闭LED2
{
	GPIO_SetBits(GPIOA,GPIO_Pin_2);
}
void LED2_Turn(void)					//切换LED2状态
{
	if(GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_2)==0)
	{
		GPIO_SetBits(GPIOA,GPIO_Pin_2);
	}else{
		GPIO_ResetBits(GPIOA,GPIO_Pin_2);
	}
}
void LED3_ON(void)					//开LED3
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_3);
}
void LED3_OFF(void)					//关闭LED3
{
	GPIO_SetBits(GPIOA,GPIO_Pin_3);
}
void LED3_Turn(void)					//切换LED3状态
{
	if(GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_3)==0)
	{
		GPIO_SetBits(GPIOA,GPIO_Pin_3);
	}else{
		GPIO_ResetBits(GPIOA,GPIO_Pin_3);
	}
}
void LED_check(void)
{
    uint8_t Round;
    for (Round = 0; Round < 3; Round++)
    {
        LED1_ON(); Delay_ms(200); LED1_OFF();
        LED2_ON(); Delay_ms(200); LED2_OFF();
        LED3_ON(); Delay_ms(200); LED3_OFF();
    }
}
