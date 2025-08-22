#include "LVGL_Example.h"
#include "adc_manager.h"
#include "config.h"
#include "network_manager.h"
#include "esp_wifi.h"
#include "driver/temperature_sensor.h"

/**********************
 *      TYPEDEFS
 **********************/
typedef enum {
    DISP_SMALL,
    DISP_MEDIUM,
    DISP_LARGE,
} disp_size_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void Onboard_create(lv_obj_t * parent);

static void ta_event_cb(lv_event_t * e);
void example1_increase_lvgl_tick(lv_timer_t * t);

// New ADC display functions
void simple_ai_display(void);
void adc_display_init(void);
void adc_display_update_timer(lv_timer_t * timer);
void adc_display_set_brightness(uint8_t brightness);
void boot_status_display_init(void);
void boot_status_update(const char* status);
void boot_wifi_status_update(void);
void boot_temp_status_update(void);
void clear_all_display_objects(void);

/**********************
 *  STATIC VARIABLES
 **********************/
static disp_size_t disp_size;

// ADC Display variables
static lv_obj_t * adc_title_label = NULL;
static lv_obj_t * adc_value_labels[CONFIG_ADC_CHANNEL_COUNT] = {NULL};
static lv_obj_t * adc_status_label = NULL;
static lv_timer_t * adc_update_timer = NULL;

// Boot Status Display variables
static lv_obj_t * boot_logo_label = NULL;
static lv_obj_t * boot_title_label = NULL;
static lv_obj_t * boot_status_label = NULL;
static lv_obj_t * boot_progress_label = NULL;
static lv_obj_t * boot_wifi_label = NULL;
static lv_obj_t * boot_temp_label = NULL;

// Temperature sensor handle
static temperature_sensor_handle_t temp_sensor = NULL;

// Live status display variables (for ADC screen)
static lv_obj_t * live_wifi_label = NULL;
static lv_obj_t * live_temp_label = NULL;
static lv_obj_t * live_logo_label = NULL;

static lv_obj_t * tv;
lv_style_t style_text_muted;
lv_style_t style_title;
static lv_style_t style_icon;
static lv_style_t style_bullet;



static const lv_font_t * font_large;
static const lv_font_t * font_normal;

static lv_timer_t * auto_step_timer;

static lv_timer_t * meter2_timer;

lv_obj_t * SD_Size;
lv_obj_t * FlashSize;
lv_obj_t * Board_angle;
lv_obj_t * RTC_Time;
lv_obj_t * Wireless_Scan;



void IRAM_ATTR auto_switch(lv_timer_t * t)
{
  uint16_t page = lv_tabview_get_tab_act(tv);

  if (page == 0) { 
    lv_tabview_set_act(tv, 1, LV_ANIM_ON); 
  } else if (page == 3) {
    lv_tabview_set_act(tv, 2, LV_ANIM_ON); 
  }
}
void Lvgl_Example1(void){

  disp_size = DISP_SMALL;                            

  font_large = LV_FONT_DEFAULT;                             
  font_normal = LV_FONT_DEFAULT;                         
  
  lv_coord_t tab_h;
  tab_h = 45;
  #if LV_FONT_MONTSERRAT_18
    font_large     = &lv_font_montserrat_18;
  #else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_18 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
  #endif
  #if LV_FONT_MONTSERRAT_12
    font_normal    = &lv_font_montserrat_12;
  #else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_12 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
  #endif
  
  lv_style_init(&style_text_muted);
  lv_style_set_text_opa(&style_text_muted, LV_OPA_90);

  lv_style_init(&style_title);
  lv_style_set_text_font(&style_title, font_large);

  lv_style_init(&style_icon);
  lv_style_set_text_color(&style_icon, lv_theme_get_color_primary(NULL));
  lv_style_set_text_font(&style_icon, font_large);

  lv_style_init(&style_bullet);
  lv_style_set_border_width(&style_bullet, 0);
  lv_style_set_radius(&style_bullet, LV_RADIUS_CIRCLE);

  tv = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, tab_h);

  lv_obj_set_style_text_font(lv_scr_act(), font_normal, 0);


  lv_obj_t * t1 = lv_tabview_add_tab(tv, "Onboard");
  
  
  Onboard_create(t1);
  
  
}

void Lvgl_Example1_close(void)
{
  /*Delete all animation*/
  lv_anim_del(NULL, NULL);

  lv_timer_del(meter2_timer);
  meter2_timer = NULL;

  lv_obj_clean(lv_scr_act());

  lv_style_reset(&style_text_muted);
  lv_style_reset(&style_title);
  lv_style_reset(&style_icon);
  lv_style_reset(&style_bullet);
}


/**********************
*   STATIC FUNCTIONS
**********************/

static void Onboard_create(lv_obj_t * parent)
{

  /*Create a panel*/
  lv_obj_t * panel1 = lv_obj_create(parent);
  lv_obj_set_height(panel1, LV_SIZE_CONTENT);

  lv_obj_t * panel1_title = lv_label_create(panel1);
  lv_label_set_text(panel1_title, "Onboard parameter");
  lv_obj_add_style(panel1_title, &style_title, 0);

  lv_obj_t * SD_label = lv_label_create(panel1);
  lv_label_set_text(SD_label, "SD Card");
  lv_obj_add_style(SD_label, &style_text_muted, 0);

  SD_Size = lv_textarea_create(panel1);
  lv_textarea_set_one_line(SD_Size, true);
  lv_textarea_set_placeholder_text(SD_Size, "SD Size");
  lv_obj_add_event_cb(SD_Size, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t * Flash_label = lv_label_create(panel1);
  lv_label_set_text(Flash_label, "Flash Size");
  lv_obj_add_style(Flash_label, &style_text_muted, 0);

  FlashSize = lv_textarea_create(panel1);
  lv_textarea_set_one_line(FlashSize, true);
  lv_textarea_set_placeholder_text(FlashSize, "Flash Size");
  lv_obj_add_event_cb(FlashSize, ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t * Wireless_label = lv_label_create(panel1);
  lv_label_set_text(Wireless_label, "Wireless scan");
  lv_obj_add_style(Wireless_label, &style_text_muted, 0);

  Wireless_Scan = lv_textarea_create(panel1);
  lv_textarea_set_one_line(Wireless_Scan, true);
  lv_textarea_set_placeholder_text(Wireless_Scan, "Wireless number");
  lv_obj_add_event_cb(Wireless_Scan, ta_event_cb, LV_EVENT_ALL, NULL);

  // 器件布局
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(parent, grid_main_col_dsc, grid_main_row_dsc);


  /*Create the top panel*/
  static lv_coord_t grid_1_col_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_1_row_dsc[] = {LV_GRID_CONTENT, /*Avatar*/
                                        LV_GRID_CONTENT, /*Name*/
                                        LV_GRID_CONTENT, /*Description*/
                                        LV_GRID_CONTENT, /*Email*/
                                        LV_GRID_CONTENT, /*Phone number*/
                                        LV_GRID_CONTENT, /*Button1*/
                                        LV_GRID_CONTENT, /*Button2*/
                                        LV_GRID_TEMPLATE_LAST
                                        };

  lv_obj_set_grid_dsc_array(panel1, grid_1_col_dsc, grid_1_row_dsc);


  static lv_coord_t grid_2_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_2_row_dsc[] = {
    LV_GRID_CONTENT,  /*Title*/
    5,                /*Separator*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_CONTENT,  /*Box title*/
    40,               /*Box*/
    LV_GRID_TEMPLATE_LAST               
  };

  // lv_obj_set_grid_dsc_array(panel2, grid_2_col_dsc, grid_2_row_dsc);
  // lv_obj_set_grid_dsc_array(panel3, grid_2_col_dsc, grid_2_row_dsc);

  lv_obj_set_grid_cell(panel1, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
  lv_obj_set_grid_dsc_array(panel1, grid_2_col_dsc, grid_2_row_dsc);
  lv_obj_set_grid_cell(panel1_title, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(SD_label, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 2, 1);
  lv_obj_set_grid_cell(SD_Size, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);
  lv_obj_set_grid_cell(Flash_label, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 4, 1);
  lv_obj_set_grid_cell(FlashSize, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 5, 1);
  lv_obj_set_grid_cell(Wireless_label, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 6, 1);
  lv_obj_set_grid_cell(Wireless_Scan, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 7, 1);

  // 器件布局 END
  
  auto_step_timer = lv_timer_create(example1_increase_lvgl_tick, 100, NULL);
}

void example1_increase_lvgl_tick(lv_timer_t * t)
{
  char buf[100]={0}; 
  
  snprintf(buf, sizeof(buf), "%ld MB\r\n", SDCard_Size);
  lv_textarea_set_placeholder_text(SD_Size, buf);
  snprintf(buf, sizeof(buf), "%ld MB\r\n", Flash_Size);
  lv_textarea_set_placeholder_text(FlashSize, buf);
  if(Scan_finish)
    snprintf(buf, sizeof(buf), "W: %d  B: %d    OK.\r\n",WIFI_NUM,BLE_NUM);
    // snprintf(buf, sizeof(buf), "WIFI: %d     ..OK.\r\n",WIFI_NUM);
  else
    snprintf(buf, sizeof(buf), "W: %d  B: %d\r\n",WIFI_NUM,BLE_NUM);
    // snprintf(buf, sizeof(buf), "WIFI: %d  \r\n",WIFI_NUM);
  lv_textarea_set_placeholder_text(Wireless_Scan, buf);
}

static void ta_event_cb(lv_event_t * e)
{
}

/**
 * Simple display function to show "AI is dumb" text
 * Based on LVGL get_started example pattern - using LVGL v8.3 API
 */
void simple_ai_display(void)
{
    // Clear the screen and set background color to dark blue
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x003a57), LV_PART_MAIN);

    // Create a white label with the text "AI is dumb"
    lv_obj_t * label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "AI is dumb");

    // Set text color to white
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), LV_PART_MAIN);

    // Center the label on the screen
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Optional: Set a larger font if available
    // lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN);
}

/**
 * Initialize ADC display with real-time voltage readings
 * Shows ADC channels and updates every second
 */
// Function to clear all display objects
void clear_all_display_objects(void)
{
    // Clear all boot objects
    if (boot_logo_label != NULL) {
        lv_obj_del(boot_logo_label);
        boot_logo_label = NULL;
    }
    if (boot_title_label != NULL) {
        lv_obj_del(boot_title_label);
        boot_title_label = NULL;
    }
    if (boot_status_label != NULL) {
        lv_obj_del(boot_status_label);
        boot_status_label = NULL;
    }
    if (boot_progress_label != NULL) {
        lv_obj_del(boot_progress_label);
        boot_progress_label = NULL;
    }
    if (boot_wifi_label != NULL) {
        lv_obj_del(boot_wifi_label);
        boot_wifi_label = NULL;
    }
    if (boot_temp_label != NULL) {
        lv_obj_del(boot_temp_label);
        boot_temp_label = NULL;
    }

    // Clear all ADC objects
    if (adc_title_label != NULL) {
        lv_obj_del(adc_title_label);
        adc_title_label = NULL;
    }
    if (adc_status_label != NULL) {
        lv_obj_del(adc_status_label);
        adc_status_label = NULL;
    }
    // Clear ADC value labels array
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        if (adc_value_labels[i] != NULL) {
            lv_obj_del(adc_value_labels[i]);
            adc_value_labels[i] = NULL;
        }
    }
    // Clear live status labels
    if (live_wifi_label != NULL) {
        lv_obj_del(live_wifi_label);
        live_wifi_label = NULL;
    }
    if (live_temp_label != NULL) {
        lv_obj_del(live_temp_label);
        live_temp_label = NULL;
    }
    if (live_logo_label != NULL) {
        lv_obj_del(live_logo_label);
        live_logo_label = NULL;
    }

    // Clean the entire screen
    lv_obj_clean(lv_scr_act());
}

void adc_display_init(void)
{
    // PROPERLY CLEAR THE SCREEN - Remove all old objects first
    clear_all_display_objects();

    // Set background color to dark blue
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x003a57), LV_PART_MAIN);

    // Create LOGulator logo at top
    live_logo_label = lv_label_create(lv_scr_act());
    lv_label_set_text(live_logo_label, "LOGulator");
    lv_obj_set_style_text_color(live_logo_label, lv_color_hex(0x00ffff), LV_PART_MAIN);
    lv_obj_align(live_logo_label, LV_ALIGN_TOP_MID, 0, 5);

    // Create title label - positioned below logo
    adc_title_label = lv_label_create(lv_scr_act());
    lv_label_set_text(adc_title_label, "ADC Readings");
    lv_obj_set_style_text_color(adc_title_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(adc_title_label, LV_ALIGN_TOP_MID, 0, 25);

    // Create labels for each ADC channel - positioned just below "ADC Readings" title
    // Using TOP_MID alignment with offset from title (title is at y=25, so start at y=50 for a bit more space)
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        if (adc_manager_is_channel_enabled(i)) {
            adc_value_labels[i] = lv_label_create(lv_scr_act());
            lv_label_set_text(adc_value_labels[i], "ADC0: -.---V");
            lv_obj_set_style_text_color(adc_value_labels[i], lv_color_hex(0x00ff00), LV_PART_MAIN);
            lv_obj_align(adc_value_labels[i], LV_ALIGN_TOP_MID, 0, 50 + (i * 25));
        }
    }

    // Create live WiFi status label - lower area (moved up to avoid clipping)
    live_wifi_label = lv_label_create(lv_scr_act());
    lv_label_set_text(live_wifi_label, "WiFi: Checking...");
    lv_obj_set_style_text_color(live_wifi_label, lv_color_hex(0xff8000), LV_PART_MAIN);
    lv_obj_align(live_wifi_label, LV_ALIGN_BOTTOM_MID, 0, -60);

    // Create live temperature label - below WiFi field to prevent RSSI clipping
    live_temp_label = lv_label_create(lv_scr_act());
    lv_label_set_text(live_temp_label, "Temp: --°C");
    lv_obj_set_style_text_color(live_temp_label, lv_color_hex(0x00ffff), LV_PART_MAIN);
    lv_obj_align(live_temp_label, LV_ALIGN_BOTTOM_MID, 0, -40);

    // Create status label - very bottom
    adc_status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(adc_status_label, "Initializing...");
    lv_obj_set_style_text_color(adc_status_label, lv_color_hex(0xffff00), LV_PART_MAIN);
    lv_obj_align(adc_status_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    // Create timer to update display every 1000ms (1 second)
    adc_update_timer = lv_timer_create(adc_display_update_timer, 1000, NULL);

    // Force initial update
    adc_display_update_timer(adc_update_timer);
}

/**
 * Timer callback to update ADC display values
 * Called every second to refresh voltage readings
 */
void adc_display_update_timer(lv_timer_t * timer)
{
    static uint32_t update_count = 0;
    char buffer[64];
    bool any_channel_active = false;

    update_count++;

    // Update each enabled ADC channel
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        if (adc_manager_is_channel_enabled(i) && adc_value_labels[i] != NULL) {
            any_channel_active = true;
            float voltage = 0.0f;

            // Get instant reading from ADC manager
            esp_err_t ret = adc_manager_get_instant_reading(i, &voltage);

            if (ret == ESP_OK) {
                // Format voltage reading
                snprintf(buffer, sizeof(buffer), "ADC%d: %5.3fV", i, voltage);
                lv_label_set_text(adc_value_labels[i], buffer);

                // Change color based on voltage level
                if (voltage > 2.5f) {
                    lv_obj_set_style_text_color(adc_value_labels[i], lv_color_hex(0xff0000), LV_PART_MAIN); // Red for high
                } else if (voltage > 1.0f) {
                    lv_obj_set_style_text_color(adc_value_labels[i], lv_color_hex(0x00ff00), LV_PART_MAIN); // Green for normal
                } else {
                    lv_obj_set_style_text_color(adc_value_labels[i], lv_color_hex(0x0080ff), LV_PART_MAIN); // Blue for low
                }
            } else {
                // Show error
                snprintf(buffer, sizeof(buffer), "ADC%d: ERROR", i);
                lv_label_set_text(adc_value_labels[i], buffer);
                lv_obj_set_style_text_color(adc_value_labels[i], lv_color_hex(0xff8000), LV_PART_MAIN); // Orange for error
            }
        }
    }

    // Update live WiFi status
    if (live_wifi_label != NULL) {
        char wifi_buffer[80];
        if (network_manager_is_wifi_connected()) {
            wifi_ap_record_t ap_info;
            esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
            if (ret == ESP_OK) {
                int rssi_percent = 100 + ap_info.rssi + 30;
                if (rssi_percent > 100) rssi_percent = 100;
                if (rssi_percent < 0) rssi_percent = 0;
                // Shorten SSID to prevent clipping and put RSSI on separate line
                snprintf(wifi_buffer, sizeof(wifi_buffer), "WiFi: %.8s\n      %ddBm", (char*)ap_info.ssid, ap_info.rssi);
                lv_obj_set_style_text_color(live_wifi_label, lv_color_hex(0x00ff00), LV_PART_MAIN);
            } else {
                snprintf(wifi_buffer, sizeof(wifi_buffer), "WiFi: Connected");
                lv_obj_set_style_text_color(live_wifi_label, lv_color_hex(0x00ff00), LV_PART_MAIN);
            }
        } else {
            snprintf(wifi_buffer, sizeof(wifi_buffer), "WiFi: Disconnected");
            lv_obj_set_style_text_color(live_wifi_label, lv_color_hex(0xff0000), LV_PART_MAIN);
        }
        lv_label_set_text(live_wifi_label, wifi_buffer);
    }

    // Update live temperature status
    if (live_temp_label != NULL) {
        char temp_buffer[32];
        float temperature = 0.0f;
        if (temp_sensor != NULL) {
            esp_err_t ret = temperature_sensor_get_celsius(temp_sensor, &temperature);
            if (ret == ESP_OK) {
                snprintf(temp_buffer, sizeof(temp_buffer), "Temp: %.1f°C", temperature);
                // Color code temperature
                if (temperature > 70.0f) {
                    lv_obj_set_style_text_color(live_temp_label, lv_color_hex(0xff0000), LV_PART_MAIN); // Red
                } else if (temperature > 50.0f) {
                    lv_obj_set_style_text_color(live_temp_label, lv_color_hex(0xff8000), LV_PART_MAIN); // Orange
                } else if (temperature > 30.0f) {
                    lv_obj_set_style_text_color(live_temp_label, lv_color_hex(0x00ff00), LV_PART_MAIN); // Green
                } else {
                    lv_obj_set_style_text_color(live_temp_label, lv_color_hex(0x00ffff), LV_PART_MAIN); // Cyan
                }
            } else {
                snprintf(temp_buffer, sizeof(temp_buffer), "Temp: Error");
                lv_obj_set_style_text_color(live_temp_label, lv_color_hex(0xff0000), LV_PART_MAIN);
            }
        } else {
            snprintf(temp_buffer, sizeof(temp_buffer), "Temp: N/A");
            lv_obj_set_style_text_color(live_temp_label, lv_color_hex(0x808080), LV_PART_MAIN);
        }
        lv_label_set_text(live_temp_label, temp_buffer);
    }

    // Update ADC status
    if (any_channel_active) {
        if (adc_manager_is_running()) {
            snprintf(buffer, sizeof(buffer), "Updates: %lu", update_count);
        } else {
            snprintf(buffer, sizeof(buffer), "ADC Stopped");
        }
    } else {
        snprintf(buffer, sizeof(buffer), "No ADC channels");
    }

    if (adc_status_label != NULL) {
        lv_label_set_text(adc_status_label, buffer);
    }
}

/**
 * Set display brightness dynamically
 * @param brightness: 0-100 (percentage)
 */
void adc_display_set_brightness(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;

    // Call the hardware brightness control function
    BK_Light(brightness);

    // Update status to show brightness change
    if (adc_status_label != NULL) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Brightness set to %d%%", brightness);
        lv_label_set_text(adc_status_label, buffer);
    }
}

/**
 * Initialize boot status display for immediate feedback
 * Shows system initialization progress with LOGulator branding - FIXED LAYOUT
 */
void boot_status_display_init(void)
{
    // Clear the screen and set background color to dark blue
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x003a57), LV_PART_MAIN);

    // Create logo label - "LOGulator" branding at top
    boot_logo_label = lv_label_create(lv_scr_act());
    lv_label_set_text(boot_logo_label, "LOGulator");
    lv_obj_set_style_text_color(boot_logo_label, lv_color_hex(0x00ffff), LV_PART_MAIN); // Cyan for branding
    lv_obj_align(boot_logo_label, LV_ALIGN_TOP_MID, 0, 15);

    // Create title label - well spaced below logo
    boot_title_label = lv_label_create(lv_scr_act());
    lv_label_set_text(boot_title_label, "ESP32-C6 DataLogger");
    lv_obj_set_style_text_color(boot_title_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(boot_title_label, LV_ALIGN_TOP_MID, 0, 50);

    // Create status label - upper middle area
    boot_status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(boot_status_label, "Initializing system...");
    lv_obj_set_style_text_color(boot_status_label, lv_color_hex(0x00ff00), LV_PART_MAIN);
    lv_obj_align(boot_status_label, LV_ALIGN_CENTER, 0, -40);

    // Create progress label - center area
    boot_progress_label = lv_label_create(lv_scr_act());
    lv_label_set_text(boot_progress_label, "Step 1/8: Hardware Init");
    lv_obj_set_style_text_color(boot_progress_label, lv_color_hex(0xffff00), LV_PART_MAIN);
    lv_obj_align(boot_progress_label, LV_ALIGN_CENTER, 0, -10);

    // Create WiFi status label - lower area, left side
    boot_wifi_label = lv_label_create(lv_scr_act());
    lv_label_set_text(boot_wifi_label, "WiFi: Init...");
    lv_obj_set_style_text_color(boot_wifi_label, lv_color_hex(0xff8000), LV_PART_MAIN); // Orange for WiFi
    lv_obj_align(boot_wifi_label, LV_ALIGN_CENTER, 0, 30);

    // Create temperature label - bottom area
    boot_temp_label = lv_label_create(lv_scr_act());
    lv_label_set_text(boot_temp_label, "Temp: --°C");
    lv_obj_set_style_text_color(boot_temp_label, lv_color_hex(0x00ffff), LV_PART_MAIN); // Cyan for temperature
    lv_obj_align(boot_temp_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    // Initialize temperature sensor
    temperature_sensor_config_t temp_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    esp_err_t ret = temperature_sensor_install(&temp_config, &temp_sensor);
    if (ret == ESP_OK) {
        temperature_sensor_enable(temp_sensor);
    }

    // Force immediate screen update
    lv_timer_handler();
}

/**
 * Update boot status display with current initialization step
 * @param status: Status message to display
 */
void boot_status_update(const char* status)
{
    if (boot_status_label != NULL && status != NULL) {
        lv_label_set_text(boot_status_label, status);

        // Update progress label with more detail
        if (boot_progress_label != NULL) {
            char progress_buffer[64];
            static uint32_t boot_time_ms = 0;
            boot_time_ms += 100; // Rough estimate

            snprintf(progress_buffer, sizeof(progress_buffer), "Boot time: %lu ms", boot_time_ms);
            lv_label_set_text(boot_progress_label, progress_buffer);
        }

        // Force immediate screen update
        lv_timer_handler();
    }
}

/**
 * Update WiFi status display with connection details
 * Shows SSID, RSSI, and connection status
 */
void boot_wifi_status_update(void)
{
    if (boot_wifi_label == NULL) return;

    char wifi_buffer[80];

    if (network_manager_is_wifi_connected()) {
        // Get WiFi info
        wifi_ap_record_t ap_info;
        esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);

        if (ret == ESP_OK) {
            // Calculate signal strength percentage (RSSI typically -30 to -90 dBm)
            int rssi_percent = 100 + ap_info.rssi + 30; // Rough conversion
            if (rssi_percent > 100) rssi_percent = 100;
            if (rssi_percent < 0) rssi_percent = 0;

            // Format: "WiFi: MyNetwork -45dBm (92%)"
            snprintf(wifi_buffer, sizeof(wifi_buffer),
                    "WiFi: %.16s %ddBm (%d%%)",
                    (char*)ap_info.ssid, ap_info.rssi, rssi_percent);

            // Green for good connection
            lv_obj_set_style_text_color(boot_wifi_label, lv_color_hex(0x00ff00), LV_PART_MAIN);
        } else {
            snprintf(wifi_buffer, sizeof(wifi_buffer), "WiFi: Connected (no details)");
            lv_obj_set_style_text_color(boot_wifi_label, lv_color_hex(0x00ff00), LV_PART_MAIN);
        }
    } else {
        // Check if WiFi scan is complete or still scanning
        if (!network_manager_is_scan_complete()) {
            snprintf(wifi_buffer, sizeof(wifi_buffer), "WiFi: Scanning networks...");
            lv_obj_set_style_text_color(boot_wifi_label, lv_color_hex(0xffff00), LV_PART_MAIN); // Yellow
        } else {
            snprintf(wifi_buffer, sizeof(wifi_buffer), "WiFi: Not connected");
            lv_obj_set_style_text_color(boot_wifi_label, lv_color_hex(0xff0000), LV_PART_MAIN); // Red
        }
    }

    lv_label_set_text(boot_wifi_label, wifi_buffer);
    lv_timer_handler(); // Force immediate update
}

/**
 * Update temperature display with current system temperature
 * Shows ESP32-C6 internal temperature sensor reading
 */
void boot_temp_status_update(void)
{
    if (boot_temp_label == NULL) return;

    char temp_buffer[32];
    float temperature = 0.0f;

    if (temp_sensor != NULL) {
        esp_err_t ret = temperature_sensor_get_celsius(temp_sensor, &temperature);

        if (ret == ESP_OK) {
            // Format temperature with color coding
            snprintf(temp_buffer, sizeof(temp_buffer), "Temp: %.1f°C", temperature);

            // Color code based on temperature
            if (temperature > 70.0f) {
                // Red for hot (>70°C)
                lv_obj_set_style_text_color(boot_temp_label, lv_color_hex(0xff0000), LV_PART_MAIN);
            } else if (temperature > 50.0f) {
                // Orange for warm (50-70°C)
                lv_obj_set_style_text_color(boot_temp_label, lv_color_hex(0xff8000), LV_PART_MAIN);
            } else if (temperature > 30.0f) {
                // Green for normal (30-50°C)
                lv_obj_set_style_text_color(boot_temp_label, lv_color_hex(0x00ff00), LV_PART_MAIN);
            } else {
                // Blue for cool (<30°C)
                lv_obj_set_style_text_color(boot_temp_label, lv_color_hex(0x00ffff), LV_PART_MAIN);
            }
        } else {
            snprintf(temp_buffer, sizeof(temp_buffer), "Temp: Error");
            lv_obj_set_style_text_color(boot_temp_label, lv_color_hex(0xff0000), LV_PART_MAIN);
        }
    } else {
        snprintf(temp_buffer, sizeof(temp_buffer), "Temp: N/A");
        lv_obj_set_style_text_color(boot_temp_label, lv_color_hex(0x808080), LV_PART_MAIN); // Gray
    }

    lv_label_set_text(boot_temp_label, temp_buffer);
    lv_timer_handler(); // Force immediate update
}





