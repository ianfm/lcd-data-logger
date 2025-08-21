idf_component_register(
    SRCS "test_main.c"
         "test_data_logger.c"
         "test_config.c"
         "test_hal.c"
         "test_managers.c"
    INCLUDE_DIRS "."
    REQUIRES unity main
)
