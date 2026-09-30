/*
 * CH32V003 软件模拟串口 - 全双工版 (9600 8E1)
 * ------------------------------------------------------------
 * 核心思路: 把“数时间”这件事从 CPU 的忙等循环里搬到硬件定时器里。
 *   - 接收: EXTI 抓起始位 -> TIM1 在每个 bit 的中点触发采样
 *   - 发送: 主循环只负责入队 -> TIM2 在每个 bit 的边界翻转 TX 电平
 *   - 收发都不再“关中断等一整帧”, 因此能连续收、连续发
 * ------------------------------------------------------------
 * 与旧版(忙等采样版)的关键区别:
 *   旧版 RX 中断里 Delay_Us 忙等约 1.1ms 才退出, 这一帧时间内
 *   无法响应下一个字节的起始位, 所以“只能收单字节”。
 *   本版 RX 中断每次只做“读一个引脚 + 移一位”, 几微秒即返回。
 */

#include "soft_uart_duplex.h"

/* ==================== 接收环形缓冲区 ==================== */
static uint8_t rx_buf[SOFT_UART_RX_BUF_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;

/* ==================== 发送环形缓冲区 ==================== */
static uint8_t tx_buf[SOFT_UART_TX_BUF_SIZE];
static volatile uint8_t tx_head = 0;
static volatile uint8_t tx_tail = 0;

/* ==================== 错误计数 ==================== */
volatile uint32_t soft_uart_parity_err = 0;
volatile uint32_t soft_uart_frame_err  = 0;
volatile uint32_t soft_uart_start_err  = 0;

/* ==================== 接收状态机 ==================== */
typedef enum {
    RX_IDLE = 0,     /* 空闲, 等起始位下降沿 */
    RX_START,        /* 半位后校验起始位中点 */
    RX_DATA,         /* 采 8 个数据位 */
    RX_PARITY,       /* 采校验位 */
    RX_STOP          /* 校验停止位 */
} rx_state_t;

static volatile rx_state_t rx_state   = RX_IDLE;
static volatile uint8_t    rx_dat     = 0;
static volatile uint8_t    rx_bit_cnt = 0;
static volatile uint8_t    rx_parity  = 0;

/* ==================== 发送状态机 ==================== */
typedef enum {
    TX_IDLE = 0,
    TX_ACTIVE
} tx_state_t;

static volatile tx_state_t tx_state   = TX_IDLE;
static volatile uint8_t    tx_dat     = 0;
static volatile uint8_t    tx_bit_cnt = 0;
static volatile uint8_t    tx_parity  = 0;

/* ==================== 引脚操作 ==================== */
#define TX_HIGH()  GPIO_SetBits(SOFT_UART_TX_PORT, SOFT_UART_TX_PIN)
#define TX_LOW()   GPIO_ResetBits(SOFT_UART_TX_PORT, SOFT_UART_TX_PIN)
#define RX_READ()  GPIO_ReadInputDataBit(SOFT_UART_RX_PORT, SOFT_UART_RX_PIN)

/* ==================== 奇偶校验 (异或折叠法) ==================== */
static uint8_t Soft_UART_Parity(uint8_t dat)
{
    uint8_t p = dat;
    p ^= p >> 4;
    p ^= p >> 2;
    p ^= p >> 1;
    p &= 0x01;              /* 只保留最低位, 擦掉高位草稿 */
#if (SOFT_UART_PARITY == SOFT_UART_PARITY_ODD)
    return !p;              /* 奇校验取反 */
#else
    return p;               /* 偶校验直接用 */
#endif
}

/* ==================== 接收: 复位并重新武装 EXTI ==================== */
static void Soft_UART_RX_Reset(void)
{
    TIM_Cmd(SOFT_UART_RX_TIM, DISABLE);
    TIM_SetCounter(SOFT_UART_RX_TIM, 0);
    rx_state   = RX_IDLE;
    rx_bit_cnt = 0;
    rx_dat     = 0;
    EXTI_ClearITPendingBit(SOFT_UART_RX_EXTI_LINE);
    EXTI->INTENR |= SOFT_UART_RX_EXTI_LINE;      /* 允许下一个起始位 */
}

/* ==================== 发送: 从队列取出一个字节并启动 ==================== */
static void Soft_UART_TX_Start(void)
{
    if (tx_tail == tx_head) { tx_state = TX_IDLE; return; }

    tx_dat     = tx_buf[tx_tail];
    tx_tail    = (uint8_t)((tx_tail + 1) % SOFT_UART_TX_BUF_SIZE);
    tx_parity  = Soft_UART_Parity(tx_dat);
    tx_bit_cnt = 0;
    tx_state   = TX_ACTIVE;

    TX_LOW();                                    /* 起始位拉低 */
    TIM_SetCounter(SOFT_UART_TX_TIM, 0);
    TIM_SetAutoreload(SOFT_UART_TX_TIM, BIT_TIME_US - 1);
    TIM_Cmd(SOFT_UART_TX_TIM, ENABLE);
}

/* ==================== 初始化 ==================== */
void Soft_UART_Init(void)
{
    GPIO_InitTypeDef       GPIO_InitStructure = {0};
    EXTI_InitTypeDef       EXTI_InitStructure = {0};
    NVIC_InitTypeDef       NVIC_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_InitStructure = {0};
    uint16_t psc;

    /* 定时器计数时钟: 设成 1MHz, 1 个计数 = 1us (与主频无关, 自适应) */
    psc = (uint16_t)(SystemCoreClock / 1000000) - 1;

    /* 时钟: GPIOD + AFIO + TIM1(接收, 在 APB2) + TIM2(发送, 在 APB1) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* TX: PD0 推挽输出, 空闲保持高 */
    GPIO_InitStructure.GPIO_Pin   = SOFT_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SOFT_UART_TX_PORT, &GPIO_InitStructure);
    TX_HIGH();

    /* RX: PD2 上拉输入 */
    GPIO_InitStructure.GPIO_Pin  = SOFT_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(SOFT_UART_RX_PORT, &GPIO_InitStructure);

    /* EXTI: PD2 下降沿 (只负责抓起始位) */
    GPIO_EXTILineConfig(SOFT_UART_RX_PORT_SRC, SOFT_UART_RX_PIN_SRC);
    EXTI_InitStructure.EXTI_Line    = SOFT_UART_RX_EXTI_LINE;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* TIM1: 接收采样定时器 */
    TIM_InitStructure.TIM_Prescaler     = psc;
    TIM_InitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_InitStructure.TIM_Period        = BIT_TIME_US - 1;
    TIM_InitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(SOFT_UART_RX_TIM, &TIM_InitStructure);
    TIM_ARRPreloadConfig(SOFT_UART_RX_TIM, DISABLE);    /* 改 ARR 立即生效 */
    TIM_ClearFlag(SOFT_UART_RX_TIM, TIM_FLAG_Update);
    TIM_ITConfig(SOFT_UART_RX_TIM, TIM_IT_Update, ENABLE);
    TIM_Cmd(SOFT_UART_RX_TIM, DISABLE);                 /* 空闲不计数 */

    /* TIM2: 发送移位定时器 */
    TIM_TimeBaseInit(SOFT_UART_TX_TIM, &TIM_InitStructure);
    TIM_ARRPreloadConfig(SOFT_UART_TX_TIM, DISABLE);
    TIM_ClearFlag(SOFT_UART_TX_TIM, TIM_FLAG_Update);
    TIM_ITConfig(SOFT_UART_TX_TIM, TIM_IT_Update, ENABLE);
    TIM_Cmd(SOFT_UART_TX_TIM, DISABLE);

    /* NVIC: 采样与移位都要准时, 给高优先级; EXTI 次之 */
    NVIC_InitStructure.NVIC_IRQChannel                   = SOFT_UART_RX_TIM_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel                   = SOFT_UART_TX_TIM_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel                   = SOFT_UART_RX_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_Init(&NVIC_InitStructure);

    rx_state = RX_IDLE;
    tx_state = TX_IDLE;
}

/* ==================== 发送一个字节 (非阻塞, 入队即返回) ==================== */
void Soft_UART_SendByte(uint8_t dat)
{
    uint8_t next;

    do {
        next = (uint8_t)((tx_head + 1) % SOFT_UART_TX_BUF_SIZE);
    } while (next == tx_tail);                   /* 队列满则等待发送中断腾位 */

    __disable_irq();
    tx_buf[tx_head] = dat;
    tx_head = next;
    if (tx_state == TX_IDLE) {
        Soft_UART_TX_Start();
    }
    __enable_irq();
    
}

void Soft_UART_SendString(char *str)
{
    while (*str) {
        Soft_UART_SendByte((uint8_t)(*str++));
    }
}

/* ==================== 接收读取接口 ==================== */
uint8_t Soft_UART_Available(void)
{
    return (uint8_t)(rx_head != rx_tail);
}

uint8_t Soft_UART_ReadByte(uint8_t *dat)
{
    if (rx_head == rx_tail) return 0;
    *dat    = rx_buf[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1) % SOFT_UART_RX_BUF_SIZE);
    return 1;
}

/* ==================== EXTI 中断: 只抓起始位, 不做延时 ==================== */
void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI7_0_IRQHandler(void)
{
    if (EXTI_GetITStatus(SOFT_UART_RX_EXTI_LINE) != RESET)
    {
        EXTI_ClearITPendingBit(SOFT_UART_RX_EXTI_LINE);

        if (rx_state == RX_IDLE)                 /* 只在空闲时响应, 防重复触发 */
        {
            EXTI->INTENR &= ~SOFT_UART_RX_EXTI_LINE;   /* 本帧内不再响应下降沿 */
            TIM_SetCounter(SOFT_UART_RX_TIM, 0);
            TIM_SetAutoreload(SOFT_UART_RX_TIM, HALF_BIT_TIME_US - 1); /* 先定半位 */
            TIM_Cmd(SOFT_UART_RX_TIM, ENABLE);
            rx_state   = RX_START;
            rx_bit_cnt = 0;
            rx_dat     = 0;
        }
    }
}

/* ==================== TIM1 中断: 每个 bit 中点采样 ==================== */
void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM1_UP_IRQHandler(void)
{
    if (TIM_GetITStatus(SOFT_UART_RX_TIM, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(SOFT_UART_RX_TIM, TIM_IT_Update);

        switch (rx_state)
        {
            case RX_START:
                /* 半位时刻到达, 采样起始位中点, 必须为低 */
                if (RX_READ() != 0) {
                    soft_uart_start_err++;       /* 毛刺, 丢弃 */
                    Soft_UART_RX_Reset();
                    return;
                }
                TIM_SetAutoreload(SOFT_UART_RX_TIM, BIT_TIME_US - 1); /* 改为整位 */
                rx_state   = RX_DATA;
                rx_bit_cnt = 0;
                rx_dat     = 0;
                break;

            case RX_DATA:
                if (RX_READ()) {
                    rx_dat |= (uint8_t)(1 << rx_bit_cnt);   /* LSB 在前 */
                }
                rx_bit_cnt++;
                if (rx_bit_cnt >= 8) {
#if (SOFT_UART_PARITY != SOFT_UART_PARITY_NONE)
                    rx_state = RX_PARITY;
#else
                    rx_state = RX_STOP;
#endif
                }
                break;

#if (SOFT_UART_PARITY != SOFT_UART_PARITY_NONE)
            case RX_PARITY:
                rx_parity = RX_READ() ? 1 : 0;
                if (rx_parity != Soft_UART_Parity(rx_dat)) {
                    soft_uart_parity_err++;      /* 校验错, 丢弃 */
                    Soft_UART_RX_Reset();
                    return;
                }
                rx_state = RX_STOP;
                break;
#endif

            case RX_STOP:
                if (RX_READ() == 0) {
                    soft_uart_frame_err++;       /* 帧错, 丢弃 */
                    Soft_UART_RX_Reset();
                    return;
                }
                {
                    uint8_t next = (uint8_t)((rx_head + 1) % SOFT_UART_RX_BUF_SIZE);
                    if (next != rx_tail) {       /* 未满才存 */
                        rx_buf[rx_head] = rx_dat;
                        rx_head = next;
                    }
                }
                Soft_UART_RX_Reset();            /* 立即复位, 准备接下一个字节 */
                break;

            default:
                Soft_UART_RX_Reset();
                break;
        }
    }
}

/* ==================== TIM2 中断: 每个 bit 边界翻转 TX ==================== */
void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(SOFT_UART_TX_TIM, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(SOFT_UART_TX_TIM, TIM_IT_Update);

        if (tx_state != TX_ACTIVE) {
            TIM_Cmd(SOFT_UART_TX_TIM, DISABLE);
            return;
        }

        tx_bit_cnt++;

        if (tx_bit_cnt <= 8)
        {
            if (tx_dat & 0x01) TX_HIGH();
            else               TX_LOW();
            tx_dat >>= 1;
        }
#if (SOFT_UART_PARITY != SOFT_UART_PARITY_NONE)
        else if (tx_bit_cnt == 9)
        {
            if (tx_parity) TX_HIGH();
            else           TX_LOW();
        }
        else if (tx_bit_cnt == 10)
        {
            TX_HIGH();                            /* 停止位 */
        }
#else
        else if (tx_bit_cnt == 9)
        {
            TX_HIGH();                            /* 停止位 */
        }
#endif
        else
        {
            /* 整帧发完, 停表; 队列里若还有数据, 立刻接着发下一字节 */
            TIM_Cmd(SOFT_UART_TX_TIM, DISABLE);
            TX_HIGH();
            tx_state = TX_IDLE;
            Soft_UART_TX_Start();
        }
    }
}
