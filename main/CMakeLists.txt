idf_component_register(
    SRCS 
        "main.c"
        "adc_handler.c"
        "channel_processor.c"
        "control_handler.c"
        "cli_handler.c"
        "nvs_storage.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_adc
        driver
        nvs_flash
        console
        log
        esp_system
)