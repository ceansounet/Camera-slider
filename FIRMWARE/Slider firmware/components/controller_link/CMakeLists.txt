idf_component_register(
    SRCS "controller_link.c"
    INCLUDE_DIRS "."
    REQUIRES driver motion_planner freertos log
)
