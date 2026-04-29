set(src_dirs
            LCD
            UART
            LVGL)

set(include_dirs
            LCD
            UART
            LVGL)

set(requires
            driver
            esp_timer
            esp_adc
            esp_lcd_st7789
            lvgl
            esp_lvgl_port
            esp_lcd_touch
            esp_lcd_touch_cst816s)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format -Wno-format)