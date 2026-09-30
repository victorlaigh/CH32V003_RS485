/*
频率取决于工程里 system_ch32v00x.c 的 SystemInit() → SetSysClock() 
选了哪个宏。?MRS 默认模板选的是 SYSCLK_FREQ_48MHz_HSI?：
*/
#include "debug.h"
#include "main.h"
#include <stdbool.h>
#include "functions.h"
#include "soft_uart_duplex.h"

//SWD跟PD1通的

/* Global define */
/* 引脚定义 */
#define USART_PORT GPIOD
#define USART_TX_PIN GPIO_Pin_5
#define USART_RX_PIN GPIO_Pin_6


#define DRE485_PORT GPIOA 
#define DRE485_PIN GPIO_Pin_1   //方向控制pin

#define RS485_SET_TX()    GPIO_WriteBit(DRE485_PORT, DRE485_PIN, Bit_SET)   // DE RE設為 1 (開啟發送) PA1
#define RS485_SET_RX()    GPIO_WriteBit(DRE485_PORT, DRE485_PIN, Bit_RESET) // 低电平: 接收



/* Global Variable */
vu8 val;


/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
   // main之前就会自动调用 SystemInit(); 所以这边不用
    /* 48MHz 系统时钟由 SystemInit 配置好；显式刷新全局频率并初始化延时基准，
       保证 Delay_Us 的位时间与真实主频一致 */

    /*初始化的一些东西*/
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();    //确认时钟，确保时间运算
    Delay_Init();   //初始化延时基准

    uint8_t dat;    //模拟串口测试用的变数
    __enable_irq();  // ★ 必加！开RISC-V全局中断总开关，没有这句所有中断都不工作

    Soft_UART_Init();   //初始化模拟串口
    /*while(1){Soft_UART_SendString("CH32V003 Soft UART 9600 8E1\r\n");    //测试初始化
    Delay_Ms(1000); 
    }*/

    Delay_Ms(1000); // 防磚延時，以防后面把调试口分配出去了，留一秒钟刷机

    USART_1(USART_PORT, USART_TX_PIN, USART_RX_PIN);  //初始化USART tx pd5 rx pd6 在上边define定义
    RS_485(DRE485_PORT, DRE485_PIN);  //初始化485控制脚 在transm.h设置
    RS485_SET_RX(); // DE RE設為 0 (關閉發送) PA1


    while(1)    //模拟串口测试
    {
        /* 收到数据就原样回显 */

        if(Soft_UART_Available()){    //if改成while
            Soft_UART_ReadByte(&dat);
            RS485_SET_TX();   // DE RE設為 1 (開啟發送) PA1
            while (!USART_GetFlagStatus(USART1, USART_FLAG_TXE)) {}
            USART_SendData(USART1, dat);    //发到硬件urt
            //Soft_UART_SendByte(dat);
            while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) //表示还在发
                {/* waiting for sending finish */}
        }

        /*没想到关键在这*/
        if (USART_GetFlagStatus(USART1, USART_FLAG_TC))  {
            RS485_SET_RX(); // DE RE設為 0 (關閉發送) PA1
        }

        while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET)  { 
            //while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET){}
            val = (uint8_t)USART_ReceiveData(USART1);   /* 读 DR: 清 RXNE + ORE */
            Soft_UART_SendByte(val);        //发到软件urt            /* 入队即返回, 不阻塞 */
        }   
    }
}
