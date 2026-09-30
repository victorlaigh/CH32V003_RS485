#include "ch32v00x.h" 
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