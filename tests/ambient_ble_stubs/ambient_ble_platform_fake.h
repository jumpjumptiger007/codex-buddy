#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;
enum { ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NO_MEM=1, ESP_ERR_INVALID_ARG=2,
 ESP_ERR_INVALID_STATE=3, ESP_ERR_INVALID_SIZE=4, ESP_ERR_TIMEOUT=5, ESP_ERR_NVS_NOT_FOUND=6 };
static inline void ambient_fake_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
#define ESP_LOGE(tag, ...) ambient_fake_log(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) ambient_fake_log(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) ambient_fake_log(tag, __VA_ARGS__)
const char *esp_err_to_name(int);
typedef unsigned TickType_t;
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
#define pdPASS 1
#define configMAX_PRIORITIES 25
#define NIMBLE_HS_STACK_SIZE 4096
typedef struct fake_sem { bool binary, available; } *SemaphoreHandle_t;
typedef struct fake_queue { uint8_t bytes[160]; size_t size, depth, count; } *QueueHandle_t;
typedef struct fake_task { void (*run)(void*); void *arg; bool deleted; } *TaskHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreTake(SemaphoreHandle_t,unsigned);
int xSemaphoreGive(SemaphoreHandle_t);
void vSemaphoreDelete(SemaphoreHandle_t);
QueueHandle_t xQueueCreate(size_t,size_t);
int xQueueSend(QueueHandle_t,const void*,unsigned);
int xQueueReceive(QueueHandle_t,void*,unsigned);
void vQueueDelete(QueueHandle_t);
int xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*);
void vTaskDelete(TaskHandle_t);
void vTaskSuspend(TaskHandle_t);
void *ambient_test_calloc(size_t,size_t);
void ambient_test_free(void*);
#define calloc ambient_test_calloc
#define free ambient_test_free

typedef unsigned nvs_handle_t;
#define NVS_READWRITE 1
int nvs_flash_init(void);
int nvs_open(const char*,int,nvs_handle_t*);
int nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
int nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);
int nvs_commit(nvs_handle_t);
int nvs_erase_key(nvs_handle_t,const char*);
void nvs_close(nvs_handle_t);
typedef struct { uint8_t type,val[6]; } ble_addr_t;
typedef struct { uint8_t type; } ble_uuid_t;
typedef struct { ble_uuid_t u; uint8_t value[16]; } ble_uuid128_t;
#define BLE_UUID128_INIT(...) {{128},{__VA_ARGS__}}
struct os_mbuf { uint8_t bytes[129]; uint16_t len; };
#define OS_MBUF_PKTLEN(m) ((m)->len)
struct ble_gatt_access_ctxt { uint8_t op; struct os_mbuf *om; };
struct ble_gatt_chr_def { const ble_uuid_t *uuid; int(*access_cb)(uint16_t,uint16_t,struct ble_gatt_access_ctxt*,void*); void *arg; unsigned flags; uint16_t *val_handle; };
struct ble_gatt_svc_def { int type; const ble_uuid_t *uuid; struct ble_gatt_chr_def *characteristics; };
struct ble_gatt_register_ctxt { int op; struct { const struct ble_gatt_chr_def *chr_def; uint16_t val_handle; } chr; };
struct ble_gap_conn_desc { uint16_t conn_handle; ble_addr_t peer_id_addr; struct { bool encrypted,authenticated,bonded; } sec_state; };
struct ble_gap_event {
 int type;
 struct { int status; uint16_t conn_handle; } connect;
 struct { struct ble_gap_conn_desc conn; int reason; } disconnect;
 struct { int reason; } adv_complete;
 struct { uint16_t conn_handle,attr_handle; bool cur_notify; } subscribe;
 struct { uint16_t conn_handle,channel_id,value; } mtu;
 struct { uint16_t conn_handle; int status; } enc_change;
 struct { uint16_t conn_handle; ble_addr_t peer_id_addr; } identity_resolved;
 struct { uint16_t conn_handle; struct { int action; } params; } passkey;
 struct { uint16_t conn_handle; } repeat_pairing;
};
struct ble_gap_adv_params { int conn_mode,disc_mode; };
struct ble_hs_adv_fields { unsigned flags; ble_uuid128_t *uuids128; unsigned num_uuids128,uuids128_is_complete; uint8_t *name; unsigned name_len,name_is_complete; };
struct ble_sm_io { int action; uint32_t passkey; };
struct fake_hs_cfg { void(*reset_cb)(int); void(*sync_cb)(void); void(*gatts_register_cb)(struct ble_gatt_register_ctxt*,void*); int(*store_status_cb)(void*,void*); unsigned sm_io_cap,sm_bonding,sm_mitm,sm_sc,sm_sc_only,sm_our_key_dist,sm_their_key_dist; };
extern struct fake_hs_cfg ble_hs_cfg;
enum { BLE_HS_EALREADY=10,BLE_HS_ENOTCONN,BLE_HS_EUNKNOWN, BLE_ERR_REM_USER_CONN_TERM,
 BLE_GATT_ACCESS_OP_WRITE_CHR,BLE_GATT_REGISTER_OP_CHR,BLE_GATT_SVC_TYPE_PRIMARY,
 BLE_ATT_ERR_UNLIKELY,BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN,BLE_ATT_ERR_INSUFFICIENT_AUTHEN,BLE_ATT_ERR_INSUFFICIENT_RES,
 BLE_GAP_EVENT_CONNECT,BLE_GAP_EVENT_DISCONNECT,BLE_GAP_EVENT_ADV_COMPLETE,BLE_GAP_EVENT_SUBSCRIBE,
 BLE_GAP_EVENT_MTU,BLE_GAP_EVENT_ENC_CHANGE,BLE_GAP_EVENT_IDENTITY_RESOLVED,BLE_GAP_EVENT_REPEAT_PAIRING,
 BLE_GAP_EVENT_PASSKEY_ACTION,BLE_GAP_REPEAT_PAIRING_IGNORE,BLE_SM_IOACT_DISP,BLE_SM_IO_CAP_DISP_ONLY,
 BLE_GAP_CONN_MODE_UND,BLE_GAP_DISC_MODE_GEN };
#define BLE_HS_CONN_HANDLE_NONE UINT16_MAX
#define BLE_HS_FOREVER 0
#define BLE_L2CAP_CID_ATT 4
#define BLE_HS_ADV_F_DISC_GEN 1
#define BLE_HS_ADV_F_BREDR_UNSUP 2
#define BLE_SM_PAIR_KEY_DIST_ENC 1
#define BLE_SM_PAIR_KEY_DIST_ID 2
#define BLE_GATT_CHR_F_WRITE 1
#define BLE_GATT_CHR_F_WRITE_NO_RSP 2
#define BLE_GATT_CHR_F_WRITE_ENC 4
#define BLE_GATT_CHR_F_WRITE_AUTHEN 8
#define BLE_GATT_CHR_F_NOTIFY 16
int nimble_port_init(void);
int nimble_port_deinit(void);
int nimble_port_stop(void);
void nimble_port_run(void);
int ble_gatts_count_cfg(const struct ble_gatt_svc_def*);
int ble_gatts_add_svcs(const struct ble_gatt_svc_def*);
void ble_svc_gap_init(void);
void ble_svc_gatt_init(void);
int ble_store_util_status_rr(void*,void*);
int ble_gap_conn_find(uint16_t,struct ble_gap_conn_desc*);
int ble_gap_terminate(uint16_t,int);
int ble_gap_adv_stop(void);
int ble_gap_adv_set_fields(const struct ble_hs_adv_fields*);
int ble_gap_adv_rsp_set_fields(const struct ble_hs_adv_fields*);
int ble_gap_adv_start(uint8_t,void*,int,const struct ble_gap_adv_params*,int(*)(struct ble_gap_event*,void*),void*);
int ble_gap_security_initiate(uint16_t);
int ble_sm_inject_io(uint16_t,const struct ble_sm_io*);
uint32_t esp_random(void);
int ble_hs_util_ensure_addr(int);
int ble_hs_id_infer_auto(int,uint8_t*);
int ble_hs_id_copy_addr(uint8_t,uint8_t*,void*);
int ble_svc_gap_device_name_set(const char*);
int ble_store_util_bonded_peers(ble_addr_t*,int*,int);
int ble_store_util_delete_peer(const ble_addr_t*);
struct os_mbuf *ble_hs_mbuf_from_flat(const void*,uint16_t);
int ble_hs_mbuf_to_flat(const struct os_mbuf*,void*,uint16_t,uint16_t*);
int ble_gatts_notify_custom(uint16_t,uint16_t,struct os_mbuf*);

typedef struct {
 unsigned allocations,live,fail_allocation,task_creates,fail_task,lock_calls,fail_lock,fail_lock_remaining,terminate_lock_failures,notify_lock_failures;
 bool auto_ack,run_one,mbuf_fail,has_peer,has_bond;
 int nvs_init,nvs_open,nvs_load,nvs_set,nvs_commit,nvs_erase;
 TickType_t ticks;
 int security_initiate,passkey_inject;
 int nimble_init,gatt_count,gatt_add,stop,deinit,bond_enum,bond_delete,terminate,notify;
 unsigned security_initiations,deinits,stops,terminations,notifications,nvs_closes,advertising_starts;
 bool advertising;
 uint8_t peer[7]; struct ble_gap_conn_desc desc;
} ambient_fake_platform_t;
extern ambient_fake_platform_t fake;
void ambient_fake_reset(void);
void ambient_fake_tx_once(void *ble);

/* Host tests are serialized; production uses the C3 FreeRTOS critical section. */
typedef unsigned portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0U
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))

TickType_t xTaskGetTickCount(void);
