#ifndef __BMS_CONFIG_H  // ? 1. 檢查有沒有定義過這個暗號
#define __BMS_CONFIG_H  // ? 2. 如果沒有，立刻舉起護盾

#include <stdint.h> 
#include "functions.h"
#include "main.h"

// BMS info
// example: 68 31 ce 68 02 02 60 42 75 16
// 68声明 31bms地址 ce是31取反验证
// 02功能码，2字节长 60 42  75验证 16结尾
// 31 02 02 | 2d 0f
#define BMS_Info_Operation 0x02
struct PayloadBMSInfoRequest {
    uint8_t req_0;
    uint8_t req_1;
};
_Static_assert(sizeof(struct PayloadFOCInfoRequest) == 2, "bad size");
extern const struct PayloadBMSInfoRequest PayloadBMSInfoRequestDefault;
struct PayloadBMSInfoResponse {
    uint8_t voltage_0;
    uint8_t voltage_1;
    uint8_t reserved_0;
    uint8_t reserved_1;
    uint8_t reserved_2;
    uint8_t current;
    uint8_t soc_percent;
    uint8_t reserved_3;
    uint8_t reserved_4;
    uint8_t reserved_5;
    uint8_t temperature_0;
    uint8_t temperature_1;
    uint8_t temperature_2;
    uint8_t temperature_3;
    uint8_t temperature_4;
};
_Static_assert(sizeof(struct PayloadBMSInfoResponse) == 0x0f, "bad size");

int transact_bms_info(struct PayloadBMSInfoResponse *p_response);   //指针形态, 输入门牌号


#endif //? 3. 護盾結束