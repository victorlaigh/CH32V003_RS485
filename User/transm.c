#include "main.h"
#include "debug.h"
#include "transm.h"
#include "soft_uart_duplex.h"

/*  之前有过timeout用的计时因为中断不能用卡住   */

uint8_t g_uart_send_buffer[UART_BUFFER_SIZE]; 
uint8_t g_uart_receive_buffer[UART_BUFFER_SIZE];

bool uart_receive_has_data() {
    return USART_GetFlagStatus(USART1, USART_FLAG_RXNE);
}

int transact_frame(uint8_t address, uint8_t operation, uint8_t *payload, uint8_t payload_length, uint8_t *out_payload_buffer, uint8_t out_payload_buffer_length) {
    int frame_size = build_frame_with_preamble(
        address, operation, payload, payload_length, g_uart_send_buffer, sizeof(g_uart_send_buffer)
    );  //g_uart_send_buffer是数组，本身代表&send_buffer[0]
    if (frame_size <= 0) {  // build frame failed 返回-1
        return -1;
    }

    RS485_SET_TX();   // DE RE設為 1 (開啟發送) PA1
    uart_send_frame(g_uart_send_buffer, frame_size);    //发送
    RS485_SET_RX(); // DE RE設為 0 (關閉發送) PA1
    int receive_length = uart_receive_frame_timeout(
        g_uart_receive_buffer, sizeof(g_uart_receive_buffer), RECEIVE_FIRST_TIMEOUT_MS, RECEIVE_IDLE_TIMEOUT_MS
    );
    if (receive_length <= 0) {  // receive failed 返回 -2
        return -2;
    }

    uint8_t receive_address = 0;
    uint8_t receive_operation = 0;
    int parse_result = parse_frame(
        g_uart_receive_buffer, receive_length, &receive_address, &receive_operation, out_payload_buffer, out_payload_buffer_length
    );
    if (parse_result <= 0) {     // parse failed 返回-3
        return -3; 
    }
    if (receive_address != address) { // device mismatch 返回-4
        return -4; 
    }
    if (receive_operation != (operation | 0x80)) { // operation mismatch 返回-5
        return -5;  // operation mismatch
    }

    return parse_result;  // success, parse_result is out_payload data length
}

void uart_send_frame(const uint8_t *buffer, uint8_t buffer_length) {
    for (uint8_t i = 0; i < buffer_length; ++i) {
        uart_send_byte(buffer[i]);
    }
    uart_wait_transmission_complete();  //等传输完
}

void uart_send_byte(uint8_t c) {
    while (!USART_GetFlagStatus(USART1, USART_FLAG_TXE)) {}  // wait TXE
    USART_SendData(USART1, c);
}


void uart_wait_transmission_complete() {// wait TC 等待传输完毕
    while (!USART_GetFlagStatus(USART1, USART_FLAG_TC)) {}  
}

/*指针将out_buffer传入，往后移4位 前面四个填前置*/
int build_frame_with_preamble(uint8_t address, uint8_t operation, uint8_t *payload, uint8_t payload_length, uint8_t *out_buffer, uint8_t out_buffer_length) {
    uint8_t preamble_length = 4;
    if (out_buffer_length < preamble_length) {
        return -2;
    }
    //out_buffer + prea 啥的， 为数组指针 往后移N元素 前面空出来的意思 行内计算，实时更改所以后面长度那个要减回来
    int result = build_frame(address, operation, payload, payload_length, out_buffer + preamble_length, out_buffer_length - preamble_length); 
    if (result < 0) {   //如果错误把错误码传上去
        return result;
    }
    out_buffer[0] = 0xfe;
    out_buffer[1] = 0xfe;
    out_buffer[2] = 0xfe;
    out_buffer[3] = 0xfe;
    return result + preamble_length;
}

int build_frame(uint8_t address, uint8_t operation, uint8_t *payload, uint8_t payload_length, uint8_t *out_buffer, uint8_t out_buffer_length) {
    uint8_t total_length = 4 + 1 + 1 + payload_length + 1 + 1;  // addr + operation + payload_length byte + payload_data + checksum + end
    if (total_length > out_buffer_length) { //超出能发送长度返回错误-1
        return -1;
    }

    uint8_t idx = 0;

    // address
    out_buffer[idx++] = 0x68;
    out_buffer[idx++] = address;
    out_buffer[idx++] = ~address & 0xFF;
    out_buffer[idx++] = 0x68;

    // operation
    out_buffer[idx++] = operation;

    // payload length
    out_buffer[idx++] = payload_length;

    // payload data
    for (uint8_t i = 0; i < payload_length; i++) {
        out_buffer[idx++] = payload[i] + 0x33;
    }

    // checksum
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < idx; i++) {
        checksum += out_buffer[i];
    }
    out_buffer[idx++] = checksum;

    // last byte
    out_buffer[idx++] = 0x16;

    return idx;
}

int uart_receive_frame_timeout(uint8_t *out_buffer, uint8_t out_buffer_length, uint32_t first_timeout_ms, uint32_t idle_timeout_ms) {
    uint8_t c = 0;
    bool success = false;

    if (out_buffer_length <= 0) {   //返回-1错误
        uart_wait_line_idle(idle_timeout_ms);  // wait line idle before return
        return -1;  // bad buffer length
    }

    uint8_t index = 0;

    // receive first byte use timeout first_timeout_ms (receive until first byte arrived, if not abort)
    success = uart_receive_byte_timeout(&c, first_timeout_ms);

    if (success) {  //收到第一byte后，写入[0]
        out_buffer[index++] = c;
    }
    else {  //返回-2错误
        return -2;  // first byte timeout
    }

    // receive until last byte timeout idle_timeout_ms (receive until idle)
    while (1) {
        success = uart_receive_byte_timeout(&c, idle_timeout_ms);
        if (success) {
            if (index >= out_buffer_length) {   //超出接收大小返回错误-3
                uart_wait_line_idle(idle_timeout_ms);  // wait line idle before return
                return -3;  // buffer too small
            }
            else {
                out_buffer[index++] = c;
            }
        }
        else {  //结束接收，返回长度
            // receive end
            return index;  // current index is total received length
        }
    }

    return -4;  // unreachable
}

/*因为get_ms依赖中断,中断不够用
重新写一个用delay的替代品
bool uart_receive_byte_timeout(uint8_t *out_byte, uint32_t timeout_ms) {
    uint32_t start = get_ms();

    while (get_ms() - start < timeout_ms) {
        if (uart_receive_has_data()) {
            *out_byte = USART_ReceiveData(USART1);
            return true;
        }
    }
    return false;
}*/

bool uart_receive_byte_timeout(uint8_t *out_byte, uint32_t timeout_ms) {
    uint16_t i;
    for (i = 0; i < timeout_ms; i++){   //本来的是在时间内狂试，这个等于只试了没几次
        if (uart_receive_has_data()) {
            *out_byte = USART_ReceiveData(USART1);
            return true;
        }
        Delay_Ms(1);
    }
    return false;
}

void uart_wait_line_idle(uint32_t idle_timeout_ms) {
    uint8_t c = 0;
    bool success = false;

    while (1) {
        success = uart_receive_byte_timeout(&c, idle_timeout_ms);
        if (success) {
            // still getting data, continue wait
            ;
        }
        else {
            // idle timeout, line idle
            return;
        }
    }
}

int parse_frame(uint8_t *in_buffer, uint8_t in_buffer_length, uint8_t *out_address, uint8_t *out_operation, uint8_t *out_payload_buffer, uint8_t out_payload_buffer_length) {
    
    // find the start of the frame
    uint8_t frame_start = 0;
    for (; frame_start < in_buffer_length; ++frame_start) {
        if (in_buffer[frame_start] == 0x68) {
            break;
        }
    }
    if (frame_start >= in_buffer_length) {
        return 0;
    }

    uint8_t *frame_buffer = &in_buffer[frame_start];
    uint8_t raw_frame_length = in_buffer_length - frame_start;

    // check length
    uint8_t payload_length_idx = 5;
    if (payload_length_idx >= raw_frame_length) {
        return 0;
    }
    uint8_t payload_length = frame_buffer[payload_length_idx];
    uint8_t frame_length = 4 + 1 + 1 + payload_length + 1 + 1;  // addr + operation + payload_length byte + payload_data + checksum + end
    if (frame_length > raw_frame_length) {
        return 0;
    }

    // check checksum
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < frame_length - 2; ++i) {
        checksum += frame_buffer[i];
    }
    if (checksum != frame_buffer[frame_length - 2]) {
        return 0;
    }

    // check last byte
    if (frame_buffer[frame_length - 1] != 0x16) {
        return 0;
    }

    // check address
    uint8_t first_0x68 = frame_buffer[0];
    uint8_t address = frame_buffer[1];
    uint8_t address_inv = frame_buffer[2];
    uint8_t second_0x68 = frame_buffer[3];
    if (first_0x68 != 0x68 || second_0x68 != 0x68 || address_inv != (~address & 0xFF)) {
        return 0;
    }

    // check output buffer length
    if (payload_length > out_payload_buffer_length) {
        return 0;
    }

    // extract data
    *out_address = address;
    *out_operation = frame_buffer[4];
    uint8_t payload_data_idx = 6;
    for (uint8_t i = 0; i < payload_length; ++i) {
        out_payload_buffer[i] = frame_buffer[payload_data_idx + i] - 0x33;
    }

    return payload_length;
}