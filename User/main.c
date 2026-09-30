/*
频率取决于工程里 system_ch32v00x.c 的 SystemInit() → SetSysClock() 
选了哪个宏。?MRS 默认模板选的是 SYSCLK_FREQ_48MHz_HSI?：
*/
#include "debug.h"
#include "main.h"
#include <stdbool.h>
#include "functions.h"
#include "soft_uart_duplex.h"
#include "transm.h"

//SWD跟PD1通的

/* Global define */
/* 引脚定义 */
#define USART_PORT GPIOD
#define USART_TX_PIN GPIO_Pin_5
#define USART_RX_PIN GPIO_Pin_6


#define RS485_PORT    GPIOC
#define RS485_TX_PIN  GPIO_Pin_0    //fullremap pc0 pc1
#define RS485_RX_PIN  GPIO_Pin_1


/* Global Variable */
vu8 val;


/* 核心逻辑 1：电脑发送数据过来（中断触发） */
/*
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)    //架构应为中断只管收 主循环负责发
{
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t data = USART_ReceiveData(USART1); // 读取电脑发来的指令
        //电脑发来的指令存在data
        //切换remap到rs485 设置位发送模式
        RS485_SET_TX(); // 1. 485 切换为发送模式
        //发送data

        USART_SendData(USART1, data); // 2. 硬件串口 PD5 将指令发送给 485 模块 (WCH-LinkE 也会收到，但它是输入端没影响)

        while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET); // 等待发送完成
        //发送完就切换会接收模式
        //等待接收 接受完
        //remap回平常样子
        //发送到电脑

        RS485_SET_RX(); // 3. 立即切回接收模式，等待设备响应

        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}
*/


/* 核心安全保障：等待上一次通訊徹底結束 */
void USART1_Wait_Transmission_Done(void)
{
    // TC (Transmission Complete) 為 1 代表發送移位暫存器也空了，波形已經徹底在引腳上放完
    while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}

/* 關閉所有重映射，將受影響的引腳恢復為常規高阻抗輸入（避免干擾） */
void Reset_All_Affected_Pins(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 設為浮空輸入，最安全

    // 清除 PORTD 受影響引腳
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // 清除 PORTC 受影響引腳
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 先全面關閉 AFIO 中的重映射暫存器位
    GPIO_PinRemapConfig(GPIO_PartialRemap1_USART1, DISABLE);
    GPIO_PinRemapConfig(GPIO_PartialRemap2_USART1, DISABLE);
    GPIO_PinRemapConfig(GPIO_FullRemap_USART1, DISABLE);
}

void switch_to_original(){  //tx PD5 rx PD6
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    USART1_Wait_Transmission_Done(); // 安全等待
    USART_ITConfig(USART1, USART_IT_RXNE, DISABLE); // 切換前先關閉中斷，避免干扰误判成输入
    Reset_All_Affected_Pins();       // 乾淨重置
    
    // 重新配置引腳
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // TX 必須是複用推挽
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // RX 必須是浮空輸入
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    Delay_Us(10);                                  // 6. 微小延時讓硬體電平穩定
    //USART_ReceiveData(USART1);                     // 7. ? 盲讀一次，清除可能已經產生的錯誤標誌
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // ? 8. 重新開啟中激活接收中斷
}

void switch_to_rs485(){ //tx PC0 rx PC1
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    USART1_Wait_Transmission_Done(); // 安全等待
    USART_ITConfig(USART1, USART_IT_RXNE, DISABLE); // 切換前先關閉中斷，避免干扰误判成输入
    Reset_All_Affected_Pins();       // 乾淨重置

   // 開啟方案 3 (Full Remap) 暫存器
    GPIO_PinRemapConfig(GPIO_FullRemap_USART1, ENABLE);

     // 重新配置引腳
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // TX 必須是複用推挽
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // RX 必須是浮空輸入
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    Delay_Us(10);                                  // 6. 微小延時讓硬體電平穩定
    //USART_ReceiveData(USART1);                     // 7. ? 盲讀一次，清除可能已經產生的錯誤標誌
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // ? 8. 重新開啟中激活接收中斷

    RS485_SET_RX(); // 默认初始化为接收模式
}

const struct PayloadBMSInfoRequest PayloadBMSInfoRequestDefault = {
    0x2D, 0x0F
};

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

        /*没想到关键在这 搞了两天*/
        if (USART_GetFlagStatus(USART1, USART_FLAG_TC))  {
            RS485_SET_RX(); // DE RE設為 0 (關閉發送) PA1
        }

        /* 下面这个直接改成if !=
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET){  //while改成if避免卡死
            while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET){}    //waiting for receiving finish 
            }
        val = (USART_ReceiveData(USART1));
        Soft_UART_SendByte(val);    //发到软件urt
            */
                        /*USART_GetFlagStatus(USART1, USART_FLAG_RXNE);  这个表示recieve有data*/
        while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET)  { //应该用while更合理？一次接收完 之前是if
             //Soft_UART_SendString("13\r\n");
            //while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET){}
            val = (uint8_t)USART_ReceiveData(USART1);   /* 读 DR: 清 RXNE + ORE */
            Soft_UART_SendByte(val);        //发到软件urt            /* 入队即返回, 不阻塞 */
        }   
    }
}
