idf_component_register(
    SRCS "motion_planner.c"
    INCLUDE_DIRS "."
    REQUIRES stepper nvs_flash driver freertos log
)
