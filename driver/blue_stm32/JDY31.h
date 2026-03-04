#ifndef __JDY31_H
#define __JDY31_H 			   
#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#if defined ( __CC_ARM   )
#pragma anon_unions
#endif




#define JDY31_USART(fmt, ...)  USART_printf (USART1, fmt, ##__VA_ARGS__)    
#define PC_USART(fmt, ...)       printf(fmt, ##__VA_ARGS__)       //这是串口打印函数，串口1，执行printf后会自动执行fput函数，重定向了printf。






//初始化和TCP功能函数
void JDY31_AT_Test(void);
bool JDY31_Send_AT_Cmd(char *cmd,char *ack1,char *ack2,u32 time);
void USART_printf( USART_TypeDef * USARTx, char * Data, ... );


#endif
