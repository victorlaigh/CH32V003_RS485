/*
频率取决于工程里 system_ch32v00x.c 的 SystemInit() → SetSysClock() 
选了哪个宏。?MRS 默认模板选的是 SYSCLK_FREQ_48MHz_HSI?：
*/
#include "debug.h"
#include "main.h"
#include <stdbool.h>
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
vu8 dat;     //模拟串口用的变数


int main(void)
{
    /* 48MHz 系统时钟由 SystemInit 配置好；显式刷新全局频率并初始化延时基准，
       保证 Delay_Us 的位时间与真实主频一致
       main之前就会被自动调用 SystemInit(); */

    /*初始化的一些东西*/
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();    //确认时钟，确保时间运算
    Delay_Init();   //初始化延时基准
    __enable_irq();  // RISC-V全局中断总开关，也许多余 不知道
    Delay_Ms(2000); // 防磚延時，以防后面把调试口分配出去了，留量秒钟刷机

    Soft_UART_Init();   //初始化模拟串口

    USART_1(USART_PORT, USART_TX_PIN, USART_RX_PIN);  //初始化USART tx pd5 rx pd6 在上边define定义
    RS_485(DRE485_PORT, DRE485_PIN);  //初始化485控制脚
    RS485_SET_RX(); // DE RE設為 0 (關閉發送) PA1

    while(1)    //模拟串口测试
    {
        /* 收到数据就原样回显 */

        if(Soft_UART_Available()){    //if改成while
            Soft_UART_ReadByte(&dat);
            RS485_SET_TX();   // DE RE設為 1 (開啟發送) PA1
            while (!USART_GetFlagStatus(USART1, USART_FLAG_TXE)) {}
            USART_SendData(USART1, dat);    //发到硬件urt
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


void USART_1(GPIO_TypeDef * u_port, uint16_t tx_pin, uint16_t rx_pin){
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | 
                           RCC_APB2Periph_AFIO  | // AFIO 時鐘remap需要，复位PA1 2也要
                           RCC_APB2Periph_USART1, ENABLE);

    AFIO->PCFR1 &= ~AFIO_PCFR1_PA12_REMAP;  //將 AFIO_PCFR1 暫存器的 PA12_RM 位元清零 (断开晶振切回 GPIO 模式)

    /* USART1 TX-->D.5   RX-->D.6 */
    GPIO_InitStructure.GPIO_Pin = tx_pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;   //30改成10
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; //复用推挽输出
    GPIO_Init(u_port, &GPIO_InitStructure);  //初始化 tx
    GPIO_InitStructure.GPIO_Pin = rx_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;   //浮空输入
    GPIO_Init(u_port, &GPIO_InitStructure);  //初始化 rx

    USART_InitStructure.USART_BaudRate = 9600;  //低一点相容性好点
    USART_InitStructure.USART_WordLength = USART_WordLength_9b; //8b+校验位=9b
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_Even;   //even校验
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);   //初始化UASRT

    USART_Cmd(USART1, ENABLE);  //使能串口1
}

void RS_485(GPIO_TypeDef * u_port, uint16_t dre_pin){
    GPIO_InitTypeDef  GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
                           
    GPIO_InitStructure.GPIO_Pin = dre_pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;   //也不用多块
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //推挽输出
    GPIO_Init(u_port, &GPIO_InitStructure);  //初始化 de re
}