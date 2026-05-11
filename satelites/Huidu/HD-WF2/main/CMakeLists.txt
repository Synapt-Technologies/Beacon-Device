
idf_component_register(
    SRCS "main.cpp" "lvgl_demo_ui.c"
    INCLUDE_DIRS "."
    REQUIRES esphome__esp-hub75 lvgl
)