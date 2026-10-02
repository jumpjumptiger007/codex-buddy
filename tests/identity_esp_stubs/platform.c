#define _DARWIN_C_SOURCE
#include <stdbool.h>
#include "nvs.h"
#include <assert.h>
#include <string.h>
#include <sys/random.h>
uint8_t fake_identity_record[134];
bool fake_identity_marker,fake_identity_has_key;
int fake_identity_fault;
static bool entropy;
void bootloader_random_enable(void){assert(!entropy);entropy=true;}
void bootloader_random_disable(void){assert(entropy);entropy=false;}
void esp_fill_random(void*out,size_t n){assert(entropy&&n<=256);assert(getentropy(out,n)==0);}
esp_err_t nvs_flash_init(void){return fake_identity_fault==1?ESP_FAIL:ESP_OK;}
esp_err_t nvs_open(const char*n,int mode,nvs_handle_t*out){assert(!strcmp(n,"passport_id")&&mode==NVS_READWRITE);*out=1;return fake_identity_fault==2?ESP_FAIL:ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h==1);}
esp_err_t nvs_get_u8(nvs_handle_t h,const char*k,uint8_t*out){(void)h;assert(!strcmp(k,"created"));if(fake_identity_fault==3)return ESP_FAIL;if(!fake_identity_marker)return ESP_ERR_NVS_NOT_FOUND;*out=1;return ESP_OK;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char*k,uint8_t n){(void)h;assert(!strcmp(k,"created")&&n==1);if(fake_identity_fault==4)return ESP_FAIL;fake_identity_marker=true;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char*k,void*out,size_t*n){(void)h;assert(!strcmp(k,"key"));if(fake_identity_fault==5)return ESP_FAIL;if(!fake_identity_has_key)return ESP_ERR_NVS_NOT_FOUND;assert(*n>=134);*n=134;memcpy(out,fake_identity_record,134);return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char*k,const void*p,size_t n){(void)h;assert(!strcmp(k,"key")&&n==134);if(fake_identity_fault==6)return ESP_FAIL;memcpy(fake_identity_record,p,n);fake_identity_has_key=true;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;return fake_identity_fault==7?ESP_FAIL:ESP_OK;}
