#include "JDY31.h"
#include "stm32f10x.h"
#include "string.h"
#include "delay.h"
extern uint8_t UartRxbuf[512];
extern uint16_t UartRxLen;
extern uint8_t UartRecv_Clear(void);
extern uint8_t Uart_RecvFlag(void);


//对JDY31模块发送AT指令
// cmd 待发送的指令
// ack1,ack2;期待的响应，为NULL表不需响应，两者为或逻辑关系
// time 等待响应时间
//返回1发送成功， 0失败
bool JDY31_Send_AT_Cmd(char *cmd,char *ack1,char *ack2,u32 time)
{ 
    UartRecv_Clear(); //重新接收新的数据包
    JDY31_USART("%s\r\n", cmd);
    if(ack1==0&&ack2==0)     //不需要接收数据
    {
    return true;
    }
    delay_ms(time);   //延时
		delay_ms(1000);
		if(Uart_RecvFlag()==1)
		{
		UartRxbuf[UartRxLen]='\0';
		}
    if(ack1!=0&&ack2!=0)
    {
        return ( ( bool ) strstr ( (const char*)UartRxbuf, ack1 ) || 
                         ( bool ) strstr ( (const char*)UartRxbuf, ack2 ) );
    }
    else if( ack1 != 0 ) 
        return ( ( bool ) strstr ( (const char*)UartRxbuf, ack1 ) );

    else
        return ( ( bool ) strstr ( (const char*)UartRxbuf, ack2 ) );

}





//AT
void JDY31_AT_Test(void)
{
    delay_ms(1000); 
    while(1)//如果AT指令不通过，说明硬件连接有问题
    {
        if(JDY31_Send_AT_Cmd("AT","+OK",NULL,500)) 
        {
            return;
        }
	      if(JDY31_Send_AT_Cmd("AT+DISC","+OK",NULL,500)) //有可能是原先连接着，先断开连接，再执行AT指令判断
        {
					
        }			
    }
}




//JDY31波特率设置，9600
/*
Param:(4 到 9)
4：9600
5：19200
6：38400
7：57600
8：115200
9：128000
*/
void JDY31_BaudRateSet(void)
{
		char count=0;
    delay_ms(1000); 
    while(count < 10)
    {
        if(JDY31_Send_AT_Cmd("AT+BAUD4","+OK",NULL,500)) //设置波特率为9600
        {
            return;
        }
        ++ count;
    }
      
}




//JDY31读PIN
uint8_t PIN_Buff[20];
void  JDY31_PINRead ( void )
{
    if (JDY31_Send_AT_Cmd( "AT+PIN", "+PIN=", 0, 500 ) )
    {
				memcpy(PIN_Buff,&UartRxbuf[5],UartRxLen-7);//读取PIN码，真正PIN码长度需去掉+PIN=与0x0D,0X0A长度，加起来是7字节
    }

}

static char *itoa( int value, char *string, int radix )
{
    int     i, d;
    int     flag = 0;
    char    *ptr = string;

    /* This implementation only works for decimal numbers. */
    if (radix != 10)
    {
        *ptr = 0;
        return string;
    }

    if (!value)
    {
        *ptr++ = 0x30;
        *ptr = 0;
        return string;
    }

    /* if this is a negative value insert the minus sign. */
    if (value < 0)
    {
        *ptr++ = '-';

        /* Make the value positive. */
        value *= -1;

    }

    for (i = 10000; i > 0; i /= 10)
    {
        d = value / i;

        if (d || flag)
        {
            *ptr++ = (char)(d + 0x30);
            value -= (d * i);
            flag = 1;
        }
    }

    /* Null terminate the string. */
    *ptr = 0;

    return string;

} /* NCL_Itoa */


void USART_printf ( USART_TypeDef * USARTx, char * Data, ... )
{
    const char *s;
    int d;   
    char buf[16];
	unsigned char TempData;

    va_list ap;
    va_start(ap, Data);

    while ( * Data != 0 )     // 判断数据是否到达结束符
    {                                         
        if ( * Data == 0x5c )  //'\'
        {                                     
            switch ( *++Data )
            {
                case 'r':                                     //回车符
								TempData=0x0d;
								USART_SendData(USARTx, TempData);
								while (RESET == USART_GetFlagStatus(USARTx, USART_FLAG_TC));//发送完成判断

                Data ++;
                break;

                case 'n':                                     //换行符
								TempData=0x0a;
								USART_SendData(USARTx, TempData);	
								while (RESET == USART_GetFlagStatus(USARTx, USART_FLAG_TC));//发送完成判断
								
                Data ++;
                break;

                default:
                Data ++;
                break;
            }            
        }

        else if ( * Data == '%')
        {                                     
            switch ( *++Data )
            {               
                case 's':                                         //字符串
                s = va_arg(ap, const char *);
                for ( ; *s; s++) 
                {
										TempData=*s;
								USART_SendData(USARTx, TempData);
								while (RESET == USART_GetFlagStatus(USARTx, USART_FLAG_TC));//发送完成判断

                }
                Data++;
                break;

                case 'd':           
                    //十进制
                d = va_arg(ap, int);
                itoa(d, buf, 10);
                for (s = buf; *s; s++) 
                {
                   	TempData=*s;
								USART_SendData(USARTx, TempData);
								while (RESET == USART_GetFlagStatus(USARTx, USART_FLAG_TC));//发送完成判断

                }
                     Data++;
                     break;
                default:
                     Data++;
                     break;
            }        
        }
        else 
				{
										TempData=*Data++;
								USART_SendData(USARTx, TempData);
								while (RESET == USART_GetFlagStatus(USARTx, USART_FLAG_TC));//发送完成判断

				}

    }
}



