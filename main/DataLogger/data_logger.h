#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Data Logger Core Interface

esp_err_t data_logger_init(void);
esp_err_t data_logger_start(void);
esp_err_t data_logger_stop(void);
esp_err_t data_logger_deinit(void);

// Status and Testing
esp_err_t data_logger_print_status(void);
esp_err_t data_logger_run_self_test(void);
esp_err_t data_logger_run_full_test_suite(void);
bool data_logger_is_running(void);

#ifdef __cplusplus
}
#endif
