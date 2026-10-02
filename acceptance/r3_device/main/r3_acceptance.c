/* Separate, explicitly authorized physical acceptance app. No Codex contents. */
#include <stdatomic.h>
#include <inttypes.h>
#include <stdio.h>
#include "ambient_identity.h"
#include "ambient_authenticated_session.h"
#include "ambient_session_ble.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "r3_policy.h"

static TaskHandle_t worker;
static QueueHandle_t events;
static atomic_bool pairing_armed;
static ambient_ble_t *ble;
static ambient_authenticated_session_t session;
static lv_obj_t *label;
typedef struct { unsigned kind; uint32_t passkey; } event_t;
static void wake(void *context) { (void)context; if(worker)xTaskNotifyGive(worker); }
static void changed(void *context,const ambient_ble_link_facts_t *facts)
{ (void)facts;wake(context); }
static void button(bsp_btn_t btn,bsp_btn_ev_t ev,void *context)
{
    (void)context;
    if(btn==BSP_BTN_OK && ev==BSP_BTN_CLICK){event_t e={1,0};(void)xQueueSend(events,&e,0);wake(NULL);}
    if(btn==BSP_BTN_DOWN && ev==BSP_BTN_LONG){event_t e={4,0};(void)xQueueSend(events,&e,0);wake(NULL);}
    if(btn==BSP_BTN_DOWN && ev==BSP_BTN_CLICK){event_t e={5,0};(void)xQueueSend(events,&e,0);wake(NULL);}
    /* UP long is an explicit acceptance-only local UNPAIR; never factory reset. */
    if(btn==BSP_BTN_UP && ev==BSP_BTN_LONG){event_t e={3,0};(void)xQueueSend(events,&e,0);wake(NULL);}
}
static bool pairing(void *context,const uint8_t peer[6],uint8_t type,uint32_t passkey)
{
    (void)context;(void)peer;(void)type;
    if(!atomic_exchange(&pairing_armed,false))return false;
    event_t e={2,passkey};
    bool accepted=xQueueSend(events,&e,0)==pdTRUE;wake(NULL);return accepted;
}
static void fatal_wait(void) {
    /* BSP callbacks already own this task handle: retain it until reboot. */
    for(;;)(void)ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
}
static void display(const char *text)
{
    if(label && bsp_lvgl_lock(20)){lv_label_set_text(label,text);bsp_lvgl_unlock();}
}
void app_main(void)
{
    ambient_identity_esp_t *identity=NULL;
    /* Own early entropy before RF, display, ADC/buttons, audio or BSP startup. */
    if(!ambient_identity_esp_open(&identity)){ESP_LOGE("r3","failure=IDENTITY");return;}
    uint8_t key[65];char fingerprint[25];ambient_auth_crypto_t crypto=ambient_identity_esp_crypto(identity);
    if(!ambient_identity_esp_public(identity,key)||!ambient_auth_fingerprint(&crypto,key,fingerprint)){
        ambient_identity_esp_close(identity);return;
    }
    ESP_LOGI("r3","public_fingerprint=%s",fingerprint);
    worker=xTaskGetCurrentTaskHandle();events=xQueueCreate(4,sizeof(event_t));
    if(!events){ambient_identity_esp_close(identity);return;}
    if(bsp_display_init()!=ESP_OK || !bsp_lvgl_init() || !bsp_lvgl_lock(100))return;
    label=lv_label_create(lv_screen_active());lv_obj_set_width(label,220);lv_obj_center(label);
    char intro[110];snprintf(intro,sizeof(intro),"R3 acceptance\nFingerprint: %s\nOK: arm one pairing (10s)",fingerprint);
    lv_label_set_text(label,intro);bsp_lvgl_unlock();
    bsp_display_backlight(40);
    if(bsp_button_init(button,NULL)!=ESP_OK)return;
    ESP_LOGI("r3","phase=PRE_BLE heap=%u minimum=%u largest=%u worker_stack=%u",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),(unsigned)uxTaskGetStackHighWaterMark(NULL));
    ambient_ble_config_t config={.advertising_name="Passport-R3-Accept",.link_changed=changed,
        .rx_available=wake,.pairing_approval=pairing};
    if(ambient_ble_start(&config,&ble)!=ESP_OK){ESP_LOGE("r3","failure=BLE_START");if(ble)(void)ambient_ble_stop(ble);fatal_wait();}
    ambient_transport_t transport;
    if(!ambient_session_ble_transport_init(ble,&transport))fatal_wait();
    uint64_t failed=0, last_generation=0, auth_count=0;int64_t arm_until=0, deadline=0, report=0;
    bool stopped=false;
    ambient_auth_state_t last_state=AMBIENT_AUTH_CLOSED;
    for(;;){ /* Persistent worker: every iteration has bounded work and a wait. */
        (void)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(50));int64_t now=esp_timer_get_time()/1000;
        event_t e;
        for(unsigned i=0;i<4 && xQueueReceive(events,&e,0)==pdTRUE;i++){
            if(e.kind==1){atomic_store(&pairing_armed,true);arm_until=now+10000;display("One pairing armed (10 seconds)");}
            if(e.kind==2){char text[100];snprintf(text,sizeof(text),"R3 passkey: %06" PRIu32 "\nFingerprint: %s",e.passkey,fingerprint);display(text);ESP_LOGI("r3","phase=PAIRING_APPROVED");}
            if(e.kind==3 && ble && !stopped){atomic_store(&pairing_armed,false);ambient_authenticated_session_close(&session);
                esp_err_t err=ambient_ble_unpair_local(ble);ESP_LOGI("r3","phase=LOCAL_UNPAIR result=%d",err);}
            if(e.kind==4 && ble){
                stopped=true;atomic_store(&pairing_armed,false);ambient_authenticated_session_close(&session);
                esp_err_t result=ambient_ble_stop(ble);if(result==ESP_OK)ble=NULL;
                ESP_LOGI("r3","phase=POST_TEARDOWN result=%d heap=%u minimum=%u largest=%u worker_stack=%u",result,
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),(unsigned)uxTaskGetStackHighWaterMark(NULL));
                display(result==ESP_OK?"R3 stopped: reboot to re-enter":"Stop failed: DOWN long retries");
            }
            if(e.kind==5 && ble && !stopped){
                ambient_ble_link_facts_t current;
                if(ambient_ble_get_link_facts(ble,&current)==ESP_OK &&
                    ambient_auth_complete(&session.auth,current.link_incarnation)){
                    /* Deliberately invalid bounded byte stream, never semantic truth. */
                    uint8_t invalid[129]={0};invalid[128]='\n';
                    esp_err_t result=ambient_ble_send(ble,invalid,sizeof(invalid));
                    ESP_LOGI("r3","phase=MAX_FRAME_FAULT bytes=129 result=%d",result);
                }
            }
        }
        if(stopped)continue;
        if(now>=arm_until)atomic_store(&pairing_armed,false);
        if(now>=report){report=now+1000;ambient_ble_metrics_t metrics;
            if(ambient_ble_get_metrics(ble,&metrics)==ESP_OK)ESP_LOGI("r3",
                "heap=%u minimum=%u largest=%u worker_stack=%u tx_stack=%" PRIu32 " host_stack=%" PRIu32 " rx_high=%u tx_high=%u mbuf=UNAVAILABLE snapshot_ack=%u term_attempts=%u term_fault=%u",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),(unsigned)uxTaskGetStackHighWaterMark(NULL),
                metrics.tx_stack_free_min_bytes,metrics.host_stack_free_min_bytes,(unsigned)metrics.rx_high_water_bytes,
                metrics.tx_high_water_slots,session.session.snapshot_acknowledged,metrics.termination_attempts,metrics.termination_fault);
        }
        ambient_ble_link_facts_t facts;
        if(ambient_ble_get_link_facts(ble,&facts)!=ESP_OK){ambient_authenticated_session_close(&session);continue;}
        ambient_session_link_t link=r3_acceptance_link(&facts);
        if(!r3_acceptance_ble_authorized(&link) || (session.auth.incarnation && session.auth.incarnation!=link.link_incarnation))
            ambient_authenticated_session_close(&session);
        if(!r3_acceptance_ble_authorized(&link))continue;
        ESP_LOGD("r3","phase=SECURE_LINK mtu=%u",facts.att_mtu);
        if(session.auth.state==AMBIENT_AUTH_CLOSED && failed!=link.link_incarnation){
            if(!ambient_authenticated_session_open(&session,&link,key,true,&crypto)){failed=link.link_incarnation;continue;}
            auth_count++;deadline=now+10000;ESP_LOGI("r3","phase=AUTH_OPEN reconnect=%" PRIu64 " mtu=%u",auth_count,facts.att_mtu);
        }
        if(failed==link.link_incarnation)continue;
        uint8_t bytes[129];size_t n=0;
        for(unsigned i=0;i<4;i++){
            if(ambient_ble_read(ble,bytes,sizeof(bytes),&n)!=ESP_OK || !n)break;
            /* Refresh policy immediately before dispatch; never use callback facts. */
            ambient_ble_link_facts_t current;
            if(ambient_ble_get_link_facts(ble,&current)!=ESP_OK || current.link_incarnation!=facts.link_incarnation){failed=link.link_incarnation;break;}
            link=r3_acceptance_link(&current);
            for(size_t offset=0;offset<n;offset++){
                bool before=ambient_auth_complete(&session.auth,link.link_incarnation);
                if(!ambient_authenticated_session_feed(&session,&link,bytes+offset,1,&transport)){failed=link.link_incarnation;break;}
                if(!before && ambient_auth_complete(&session.auth,link.link_incarnation))
                    ESP_LOGI("r3","phase=CONFIRM_RECEIVED before_hello=%u",session.session.generation==0);
            }
            if(failed==link.link_incarnation)break;
        }
        ambient_transport_result_t result=ambient_authenticated_session_flush(&session,&link,&transport);
        if((result!=AMBIENT_TRANSPORT_OK && result!=AMBIENT_TRANSPORT_WOULD_BLOCK) ||
            (now>=deadline && !session.session.snapshot_acknowledged))failed=link.link_incarnation;
        if(failed==link.link_incarnation){ambient_authenticated_session_close(&session);ESP_LOGW("r3","failure=AUTH_OR_SESSION");continue;}
        if(last_state!=session.auth.state){last_state=session.auth.state;ESP_LOGI("r3","phase=AUTH state=%u",(unsigned)last_state);}
        if(session.session.generation!=last_generation){last_generation=session.session.generation;ESP_LOGI("r3","generation=%" PRIu64,last_generation);}

    }
}
