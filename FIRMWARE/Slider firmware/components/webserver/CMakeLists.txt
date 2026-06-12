idf_component_register(
    SRCS "webserver.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server motion_planner stepper driver freertos log
)
