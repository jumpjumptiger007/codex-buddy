#include "bsp_display.h"
#include "passport_ui_model.h"
#include "passport_ui_shell.h"

#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "passport_app";

static passport_ui_shell_t s_shell = PASSPORT_UI_SHELL_INITIALIZER;
static passport_ui_view_t s_initial_view;

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Passport product UI");

    esp_err_t err = bsp_display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Display initialization failed: %s", esp_err_to_name(err));
        return;
    }

    if (!bsp_lvgl_init()) {
        ESP_LOGE(TAG, "LVGL initialization failed");
        return;
    }

    bsp_display_backlight(100);
    passport_ui_view_init(&s_initial_view);

    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "Could not lock LVGL for product shell initialization");
        return;
    }

    const bool shell_created =
        passport_ui_shell_create(&s_shell, &s_initial_view);
    bsp_lvgl_unlock();

    if (!shell_created) {
        ESP_LOGE(TAG, "Passport product shell initialization failed");
        return;
    }

    ESP_LOGI(TAG, "Passport product shell ready (%u persistent objects)",
             (unsigned)PASSPORT_UI_SHELL_OBJECT_COUNT);
}
