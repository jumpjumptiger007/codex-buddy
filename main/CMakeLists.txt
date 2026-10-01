idf_component_register(
    SRCS "main.c"
         "passport_ui_model.c"
         "passport_ui_shell.c"
    INCLUDE_DIRS "."
    REQUIRES bsp
)
