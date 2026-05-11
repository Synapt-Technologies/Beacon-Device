
idf_component_register(
    SRCS "main.cpp" "lvgl_demo_ui.c"
    INCLUDE_DIRS "."
    REQUIRES 
        beacon_device    
        esp-hub75 lvgl
)
# TODO Cleanup esp-hub75 lvgl