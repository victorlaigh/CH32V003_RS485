#ifndef __SOFT_UART_DUPLEX_H
#define __SOFT_UART_DUPLEX_H

#include "ch32v00x.h"

/* ==================== 配置区 ==================== */
#define SOFT_UART_BAUDRATE      9600

/* TX 引脚: PD0 (推挽输出, 空闲高) */
#define SOFT_UART_TX_PORT       GPIOD
#define SOFT_UART_TX_PIN        GPIO_Pin_0

/* RX 引脚: PD2 (已从 PD1 换开, 避开 SWIO 下载口) */
#define SOFT_UART_RX_PORT       GPIOD
#define SOFT_UART_RX_PIN        GPIO_Pin_2
#define SOFT_UART_RX_PORT_SRC   GPIO_PortSourceGPIOD    //中断port
#define SOFT_UART_RX_PIN_SRC    GPIO_PinSource2 //中断pin
#define SOFT_UART_RX_EXTI_LINE  EXTI_Line2  //中断控制线
#define SOFT_UART_RX_IRQn       EXTI7_0_IRQn    //中断口 0-7共用

/* 接收采样定时器 (只负责“采”, 不在中断里忙等) */
#define SOFT_UART_RX_TIM        TIM1
#define SOFT_UART_RX_TIM_IRQn   TIM1_UP_IRQn

/* 发送移位定时器 (只负责“发”, 不在主循环里忙等) */
#define SOFT_UART_TX_TIM        TIM2
#define SOFT_UART_TX_TIM_IRQn   TIM2_IRQn

/* 位时间: 9600bps -> 104us (定时器按 1MHz 计数, 1 tick = 1us) */
#define BIT_TIME_US             104
#define HALF_BIT_TIME_US        52

/* 收发缓冲区大小 (必须为 2 的幂次或能被整除, 这里用取模实现) */
#define SOFT_UART_RX_BUF_SIZE   64
#define SOFT_UART_TX_BUF_SIZE   64

/* 校验方式: 0=无 1=偶 2=奇 */
#define SOFT_UART_PARITY_NONE   0
#define SOFT_UART_PARITY_EVEN   1
#define SOFT_UART_PARITY_ODD    2
#define SOFT_UART_PARITY        SOFT_UART_PARITY_EVEN

/* ==================== 接口 ==================== */
void    Soft_UART_Init(void);
void    Soft_UART_SendByte(uint8_t dat);
void    Soft_UART_SendString(char *str);
uint8_t Soft_UART_ReadByte(uint8_t *dat);
uint8_t Soft_UART_Available(void);

/* 错误计数, 调试用: 全为 0 说明通信质量良好 */
extern volatile uint32_t soft_uart_parity_err;
extern volatile uint32_t soft_uart_frame_err;
extern volatile uint32_t soft_uart_start_err;

#endif
