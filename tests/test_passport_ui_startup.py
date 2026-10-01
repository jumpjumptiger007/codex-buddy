from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
startup = (ROOT / "main/main.c").read_text(encoding="utf-8")
component = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
shell = (ROOT / "main/passport_ui_shell.c").read_text(encoding="utf-8")

for required in (
    "bsp_display_init",
    "bsp_lvgl_init",
    "bsp_display_backlight",
    "passport_ui_view_init",
    "passport_ui_shell_create",
    "bsp_lvgl_lock",
    "bsp_lvgl_unlock",
):
    assert required in startup, f"product startup is missing {required}"

for forbidden in (
    "demo_navigation",
    "ui_pixel",
    "demo_audio",
    "demo_wifi",
    "demo_ble",
    "bsp_button_init",
    "bsp_audio_init",
    "bsp_battery_init",
    "xQueueCreate",
    "xTaskCreate",
):
    assert forbidden not in startup, f"product startup still references {forbidden}"

assert startup.index("bsp_lvgl_lock") < startup.index("passport_ui_shell_create")
assert startup.index("passport_ui_shell_create") < startup.index("bsp_lvgl_unlock")
assert "passport_ui_shell.c" in component
assert "demo_" not in component
assert "ui_pixel" not in component
assert '"bsp_pins.h"' in shell and "BSP_LCD_W, BSP_LCD_H" in shell
assert "lv_screen_load(shell->root)" in shell
assert "lv_obj_add_flag(shell->voice_panel, LV_OBJ_FLAG_HIDDEN)" in shell
assert "lv_obj_add_flag(shell->toast_panel, LV_OBJ_FLAG_HIDDEN)" in shell
assert "BSP_BTN_" not in startup + shell
assert "passport_ui_model.h" in (ROOT / "main/passport_ui_model.c").read_text(
    encoding="utf-8"
)

print("Passport UI startup contract tests: PASS")
