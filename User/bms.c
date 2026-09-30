#include "bms.h"
#include "debug.h"
//#include "functions.h"
#include "main.h"
#include "transm.h"
#include "soft_uart_duplex.h"


int transact_bms_info(struct PayloadBMSInfoResponse *p_response) {
    int out_payload_length = transact_frame(
        DEVICE_BMS, BMS_Info_Operation, (uint8_t*)&PayloadBMSInfoRequestDefault, sizeof(PayloadBMSInfoRequestDefault),
        (uint8_t*)p_response, sizeof(*p_response)
    );
    if (out_payload_length <= 0) {
        return out_payload_length;
    }
    if (out_payload_length != sizeof(*p_response)) {
        return -10;
    }

    return 1;  // success
}