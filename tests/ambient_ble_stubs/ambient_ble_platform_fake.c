#include "ambient_ble_platform_fake.h"
#undef calloc
#undef free
#include "ambient_ble_internal.h"
#include <assert.h>
ambient_fake_platform_t fake;
struct fake_hs_cfg ble_hs_cfg;
static struct fake_task tasks[2];
void ambient_fake_reset(void) {
 assert(fake.live == 0); memset(&fake,0,sizeof(fake)); fake.auto_ack=true;
 memset(&ble_hs_cfg,0,sizeof(ble_hs_cfg)); memset(tasks,0,sizeof(tasks));
 fake.desc.conn_handle=1; fake.desc.peer_id_addr.val[0]=1;
}
void *ambient_test_calloc(size_t n,size_t s) {
 if (++fake.allocations==fake.fail_allocation) return NULL;
 void *p=calloc(n,s); if(p) fake.live++; return p;
}
void ambient_test_free(void *p) { if(p) { assert(fake.live); fake.live--; free(p); } }
const char *esp_err_to_name(int e) { (void)e; return "injected"; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return ambient_test_calloc(1,sizeof(struct fake_sem)); }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { SemaphoreHandle_t s=xSemaphoreCreateMutex(); if(s)s->binary=true; return s; }
int xSemaphoreTake(SemaphoreHandle_t s,unsigned ticks) {
 if (++fake.lock_calls==fake.fail_lock || !s) return 0;
 if(fake.fail_lock_remaining){fake.fail_lock_remaining--;return 0;}
 if (!s->binary) return 1;
 if (s->available || (fake.auto_ack && ticks>=2000)) {s->available=false; return 1;} return 0;
}
int xSemaphoreGive(SemaphoreHandle_t s) { if(s)s->available=true; return 1; }
void vSemaphoreDelete(SemaphoreHandle_t s) { ambient_test_free(s); }
QueueHandle_t xQueueCreate(size_t depth,size_t size) { QueueHandle_t q=ambient_test_calloc(1,sizeof(struct fake_queue)); if(q){q->size=size;q->depth=depth;assert(size<=160 && depth==1);}return q; }
int xQueueSend(QueueHandle_t q,const void *p,unsigned t) { (void)t;if(!q||q->count==q->depth)return 0;memcpy(q->bytes,p,q->size);q->count++;return 1; }
int xQueueReceive(QueueHandle_t q,void *p,unsigned t) { (void)t;if(!q||!q->count){if(t){fake.ticks+=t;if(fake.run_one && g_ambient_ble_active)g_ambient_ble_active->shutting_down=true;}return 0;}memcpy(p,q->bytes,q->size);q->count--;return 1; }
void vQueueDelete(QueueHandle_t q) { ambient_test_free(q); }
int xTaskCreate(void(*run)(void*),const char*n,unsigned s,void*arg,unsigned pri,TaskHandle_t*out) {
 (void)n;(void)s;(void)pri;unsigned index=++fake.task_creates;if(index==fake.fail_task)return 0;
 struct fake_task *task=&tasks[index==1?0:1]; task->run=run;task->arg=arg;*out=task;return 1;
}
void vTaskDelete(TaskHandle_t t) { if(t)t->deleted=true; }
void vTaskSuspend(TaskHandle_t t) { (void)t; }
void ambient_fake_tx_once(void *context) {ambient_ble_t *ble=context;fake.run_one=true;ambient_ble_tx_task(ble);fake.run_one=false;ble->shutting_down=false;}
int nvs_flash_init(void){return fake.nvs_init;}
int nvs_open(const char*n,int mode,nvs_handle_t*out){(void)n;(void)mode;*out=1;return fake.nvs_open;}
int nvs_get_blob(nvs_handle_t h,const char*k,void*out,size_t*n){(void)h;assert(!strcmp(k,"accepted_peer"));if(fake.nvs_load)return fake.nvs_load;if(!fake.has_peer)return ESP_ERR_NVS_NOT_FOUND;memcpy(out,fake.peer,*n);return 0;}
int nvs_set_blob(nvs_handle_t h,const char*k,const void*p,size_t n){(void)h;assert(!strcmp(k,"accepted_peer")&&n==7);if(fake.nvs_set)return fake.nvs_set;memcpy(fake.peer,p,n);return 0;}
int nvs_commit(nvs_handle_t h){(void)h;return fake.nvs_commit;}
int nvs_erase_key(nvs_handle_t h,const char*k){(void)h;assert(!strcmp(k,"accepted_peer"));if(fake.nvs_erase)return fake.nvs_erase;fake.has_peer=false;return 0;}
void nvs_close(nvs_handle_t h){(void)h;fake.nvs_closes++;}
int nimble_port_init(void){return fake.nimble_init;}
int nimble_port_deinit(void){fake.deinits++;return fake.deinit;}
int nimble_port_stop(void){fake.stops++;return fake.stop;}
void nimble_port_run(void){}
int ble_gatts_count_cfg(const struct ble_gatt_svc_def*s){(void)s;return fake.gatt_count;}
int ble_gatts_add_svcs(const struct ble_gatt_svc_def*s){(void)s;return fake.gatt_add;}
void ble_svc_gap_init(void){}
void ble_svc_gatt_init(void){}
int ble_store_util_status_rr(void*a,void*b){(void)a;(void)b;return 0;}
int ble_gap_conn_find(uint16_t h,struct ble_gap_conn_desc*out){if(h!=fake.desc.conn_handle)return BLE_HS_ENOTCONN;*out=fake.desc;return 0;}
int ble_gap_terminate(uint16_t h,int reason){(void)h;(void)reason;fake.terminations++;fake.fail_lock_remaining=fake.terminate_lock_failures;return fake.terminate;}
int ble_gap_adv_stop(void){fake.advertising=false;return 0;}
int ble_gap_adv_set_fields(const struct ble_hs_adv_fields*a){(void)a;return 0;}
int ble_gap_adv_rsp_set_fields(const struct ble_hs_adv_fields*a){(void)a;return 0;}
int ble_gap_adv_start(uint8_t t,void*p,int timeout,const struct ble_gap_adv_params*a,int(*cb)(struct ble_gap_event*,void*),void*ctx){(void)t;(void)p;(void)timeout;(void)a;(void)cb;(void)ctx;fake.advertising=true;fake.advertising_starts++;return 0;}
int ble_gap_security_initiate(uint16_t h){(void)h;fake.security_initiations++;return fake.security_initiate;}
int ble_sm_inject_io(uint16_t h,const struct ble_sm_io*i){(void)h;(void)i;return fake.passkey_inject;}
uint32_t esp_random(void){return 42;}
int ble_hs_util_ensure_addr(int t){(void)t;return 0;}
int ble_hs_id_infer_auto(int t,uint8_t*out){(void)t;*out=0;return 0;}
int ble_hs_id_copy_addr(uint8_t t,uint8_t*out,void*p){(void)t;(void)p;memset(out,1,6);return 0;}
int ble_svc_gap_device_name_set(const char*n){(void)n;return 0;}
int ble_store_util_bonded_peers(ble_addr_t*p,int*n,int cap){(void)cap;if(fake.bond_enum)return fake.bond_enum;*n=fake.has_bond?1:0;if(*n)p[0]=fake.desc.peer_id_addr;return 0;}
int ble_store_util_delete_peer(const ble_addr_t*p){(void)p;if(!fake.bond_delete)fake.has_bond=false;return fake.bond_delete;}
static struct os_mbuf mbuf;
struct os_mbuf *ble_hs_mbuf_from_flat(const void*p,uint16_t n){if(fake.mbuf_fail)return NULL;assert(n<=129);memcpy(mbuf.bytes,p,n);mbuf.len=n;return &mbuf;}
int ble_hs_mbuf_to_flat(const struct os_mbuf*m,void*out,uint16_t cap,uint16_t*n){if(m->len>cap)return 1;memcpy(out,m->bytes,m->len);*n=m->len;return 0;}
int ble_gatts_notify_custom(uint16_t h,uint16_t a,struct os_mbuf*m){(void)h;(void)a;(void)m;fake.notifications++;fake.fail_lock_remaining=fake.notify_lock_failures;return fake.notify;}

TickType_t xTaskGetTickCount(void){return fake.ticks;}
