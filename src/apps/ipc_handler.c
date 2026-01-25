#include "apps/ipc_handler.h"
#include "apps/ipc_common.h"
#include "apps/apps_types.h"

int apps_ipc_handler(apps_t *apps, uint8_t* rxbuf, size_t rxlen,uint8_t* txbuf, size_t txcap){
    ipc_req_t *req = (ipc_req_t *)rxbuf;
    ipc_resp_t *resp = (ipc_resp_t *)txbuf;
    size_t rx_expected_len = calculate_ipc_req_size(req->type);
    if (rxlen != rx_expected_len) {
        resp->type = IPC_RESP_ERROR_LENGTH_MISMATCH;
        return sizeof(ipc_resp_type_t);
    }

    return sizeof(ipc_resp_type_t);
}