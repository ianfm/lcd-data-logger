#pragma once

#include "lvgl.h"
#include "demos/lv_demos.h"

#include "LVGL_Driver.h"
#include "SD_SPI.h"
#include "network_manager.h"  // Changed from Wireless.h - now using unified network manager

#define EXAMPLE1_LVGL_TICK_PERIOD_MS  1000


void Lvgl_Example1(void);
void simple_ai_display(void);
void adc_display_init(void);