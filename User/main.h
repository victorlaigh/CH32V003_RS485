#ifndef __MAIN_CONFIG_H  // ? 1. 檢查有沒有定義過這個暗號
#define __MAIN_CONFIG_H  // ? 2. 如果沒有，立刻舉起護盾
#include <stdint.h>
#include <stdbool.h>

void USART_1(
    GPIO_TypeDef * u_port,
    uint16_t tx_pin,
    uint16_t rx_pin
    );

void RS_485(
    GPIO_TypeDef * u_port,
    uint16_t dre_pin
    );

#endif // __MAIN_CONFIG_H // ? 3. 護盾結束