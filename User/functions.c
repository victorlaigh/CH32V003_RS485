#include "debug.h"


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

    /* 中断暂时用不到，以后可能可以试试用中断接受资料
    // ? 1. 開啟串口內部的 RXNE 中斷開關
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    // ? 2. 配置 NVIC 中斷通道（開通大閘門）
    NVIC_InitTypeDef NVIC_InitStructure = {0};
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;          // 指定為 USART1 中斷通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  // 搶佔優先級
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;         // 子優先級
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;            // 啟用通道
    NVIC_Init(&NVIC_InitStructure);
    */

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


/*

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | 
                           RCC_APB2Periph_GPIOC | 
                           RCC_APB2Periph_GPIOD | 
                           RCC_APB2Periph_AFIO  | // AFIO 時鐘remap需要，复位PA1 2也要
                           RCC_APB2Periph_USART1, ENABLE);


    //配置脚位 tx rx  485tx 485rx 485控制   
    GPIO_InitStructure.GPIO_Pin = RS485_TRE_PIN;          // 使用 PA1 控制 RE/DE 在上面定义
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    // 推挽輸出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(RS485_TRE_PORT, &GPIO_InitStructure);   //PortA 也是上面定义
    

    RS485_SET_RX(); // 默认初始化为接收模式
    */