#ifndef __MAIN_CONFIG_H  // ? 1. 檢查有沒有定義過這個暗號
#define __MAIN_CONFIG_H  // ? 2. 如果沒有，立刻舉起護盾
#include <stdint.h>
#include <stdbool.h>

enum {
    DEVICE_BMS = 0x31,
    DEVICE_FOC = 0x20,
    DEVICE_DASH = 0x10
};

// FOC info
// example: 68 20 df 68 02 02 38 3b 46 16
// 20 02 02 | 05 08
#define FOC_Info_Operation 0x02
struct PayloadFOCInfoRequest {
    uint8_t req_0;
    uint8_t req_1;
};
_Static_assert(sizeof(struct PayloadFOCInfoRequest) == 2, "bad size");
extern const struct PayloadFOCInfoRequest PayloadFOCInfoRequestDefault;
// TODO FOC: speed, drive_mode flag, parking flag, enery recovery flag, eco flag, cruise flag
struct PayloadFOCInfoResponse {
    uint8_t reserved_0;
    uint8_t reserved_1;
    uint8_t reserved_2;
    uint8_t flag_0;
    uint8_t reserved_4;
    uint8_t reserved_5;
    uint8_t reserved_6;
    uint8_t reserved_7;
    uint8_t reserved_8;
    uint8_t speed;  // signed int8_t, will be negative when reverse
    uint8_t flag_1;
    uint8_t reserved_11;
};
_Static_assert(sizeof(struct PayloadFOCInfoResponse) == 0x0c, "bad size");
enum {
    FOC_INFO_FLAG_0_DRIVE_MODE_1 = 0x01,
    FOC_INFO_FLAG_0_DRIVE_MODE_2 = 0x02,
    FOC_INFO_FLAG_1_PARKING = 0x08,
    FOC_INFO_FLAG_1_ENERGY_RECOVERY = 0x10
};


//common
#ifndef USER_COMMON_H_
#define USER_COMMON_H_
#include <stdint.h>
// milliseconds since start
extern volatile uint32_t g_ms;

// init tim2 timer. use g_ms or get_ms() get the milliseconds since start
void init_tim2_timer_interrupt();

// milliseconds since start
uint32_t get_ms();

// delay ms
void delay_ms(uint32_t ms);
#endif


uint32_t millis(void);


#endif // __MAIN_CONFIG_H // ? 3. 護盾結束