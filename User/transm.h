#ifndef TRANS_M_
#define TRANS_M_

#define RECEIVE_FIRST_TIMEOUT_MS 100    //接收第一个byte的超时
#define RECEIVE_IDLE_TIMEOUT_MS 10      //收到第一个后，后续每一个的超时

#define UART_BUFFER_SIZE 128

#define DRE485_PORT GPIOA 
#define DRE485_PIN GPIO_Pin_1

#define RS485_SET_TX()    GPIO_WriteBit(DRE485_PORT, DRE485_PIN, Bit_SET)   // DE RE設為 1 (開啟發送) PA1
#define RS485_SET_RX()    GPIO_WriteBit(DRE485_PORT, DRE485_PIN, Bit_RESET) // 低电平: 接收

extern uint8_t g_uart_send_buffer[UART_BUFFER_SIZE];
extern uint8_t g_uart_receive_buffer[UART_BUFFER_SIZE];

// send single byte without wait TC
void uart_send_byte(uint8_t c);

// send frame and wait TC
void uart_send_frame(const uint8_t *buffer, uint8_t buffer_length);

// wait TC
void uart_wait_transmission_complete();


int build_frame(uint8_t address, uint8_t operation, uint8_t *payload, uint8_t payload_length, uint8_t *out_buffer, uint8_t out_buffer_length);

int transact_frame(uint8_t address, uint8_t operation, uint8_t *payload, uint8_t payload_length, uint8_t *out_payload_buffer, uint8_t out_payload_buffer_length);

int build_frame_with_preamble(uint8_t address, uint8_t operation, uint8_t *payload, uint8_t payload_length, uint8_t *out_buffer, uint8_t out_buffer_length);

// receive frame, first byte use timeout first_timeout_ms, following bytes use timeout idle_timeout_ms
int uart_receive_frame_timeout(uint8_t *out_buffer, uint8_t out_buffer_length, uint32_t first_timeout_ms, uint32_t idle_timeout_ms);

bool uart_receive_byte_timeout(uint8_t *out_byte, uint32_t timeout_ms);

int parse_frame(uint8_t *in_buffer, uint8_t in_buffer_length, uint8_t *out_address, uint8_t *out_operation, uint8_t *out_payload_buffer, uint8_t out_payload_buffer_length);

// receive and discard until line idle
void uart_wait_line_idle(uint32_t idle_timeout_ms);

// receive has data RXNE
bool uart_receive_has_data();

#endif