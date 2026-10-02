idf_component_register(
    SRCS "main.c"
         "ambient_r3_build_seam.c"
         "passport_ui_model.c"
         "passport_ui_presenter.c"
         "passport_ui_shell.c"
    INCLUDE_DIRS "."
    REQUIRES bsp ambient_ble ambient_session
)
