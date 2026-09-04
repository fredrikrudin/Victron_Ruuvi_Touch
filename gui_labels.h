#pragma once
#include <lvgl.h>
#include <map>
#include <string>
#include <algorithm>
#include "ble_processor.h"

extern SensorData globalData;
extern SemaphoreHandle_t dataMutex;
extern std::map<std::string, std::string> victronKeys;

static lv_obj_t *tabview;
static lv_obj_t *tab_overview;
static lv_obj_t *tab_settings;

static lv_obj_t *lbl_ruuvi_temp;
static lv_obj_t *lbl_ruuvi_hum;
static lv_obj_t *lbl_victron_volt;
static lv_obj_t *lbl_victron_curr;
static lv_obj_t *lbl_victron_soc;

static lv_obj_t *list_devices;
static lv_obj_t *kb;
static lv_obj_t *ta_key;
static lv_obj_t *btn_set_shunt;
static lv_obj_t *btn_set_mppt;
static std::string selected_mac = "";

static void kb_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if(code == LV_EVENT_READY && !selected_mac.empty()) {
            const char * key_str = lv_textarea_get_text(ta_key);
            if(strlen(key_str) == 32) {
                if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    victronKeys[selected_mac] = std::string(key_str);
                    xSemaphoreGive(dataMutex);
                }
            }
        }
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ta_key, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_set_shunt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_set_mppt, LV_OBJ_FLAG_HIDDEN);
    }
}

static void role_shunt_cb(lv_event_t * e) {
    if(!selected_mac.empty() && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        globalData.shunt_mac = selected_mac;
        xSemaphoreGive(dataMutex);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ta_key, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_set_shunt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_set_mppt, LV_OBJ_FLAG_HIDDEN);
    }
}

static void role_mppt_cb(lv_event_t * e) {
    if(!selected_mac.empty() && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        globalData.mppt_mac = selected_mac;
        xSemaphoreGive(dataMutex);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ta_key, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_set_shunt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_set_mppt, LV_OBJ_FLAG_HIDDEN);
    }
}

static void device_click_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    std::string full_text = lv_list_get_btn_text(list_devices, btn);
    size_t pos = full_text.find("Victron: ");
    if(pos != std::string::npos) {
        selected_mac = full_text.substr(9);
    } else {
        selected_mac = full_text;
    }

    lv_textarea_set_text(ta_key, "");
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ta_key, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_set_shunt, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_set_mppt, LV_OBJ_FLAG_HIDDEN);
}

void init_gui(void) {
    tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 50);
    tab_overview = lv_tabview_add_tab(tabview, "📊 Översikt");
    tab_settings = lv_tabview_add_tab(tabview, "⚙️ Inställningar");

    // --- ÖVERSIKT ---
    lv_obj_t * title_ruuvi = lv_label_create(tab_overview);
    lv_label_set_text(title_ruuvi, "🌿 RuuviTag Sensor");
    lv_obj_align(title_ruuvi, LV_ALIGN_TOP_LEFT, 10, 5);

    lbl_ruuvi_temp = lv_label_create(tab_overview);
    lv_label_set_text(lbl_ruuvi_temp, "Temp: --.- °C");
    lv_obj_align(lbl_ruuvi_temp, LV_ALIGN_TOP_LEFT, 20, 25);

    lbl_ruuvi_hum = lv_label_create(tab_overview);
    lv_label_set_text(lbl_ruuvi_hum, "Fukt: --.- %");
    lv_obj_align(lbl_ruuvi_hum, LV_ALIGN_TOP_LEFT, 20, 45);

    lv_obj_t * title_victron = lv_label_create(tab_overview);
    lv_label_set_text(title_victron, "⚡ Victron System");
    lv_obj_align(title_victron, LV_ALIGN_TOP_LEFT, 10, 80);

    lbl_victron_volt = lv_label_create(tab_overview);
    lv_label_set_text(lbl_victron_volt, "Spänning: --.-- V");
    lv_obj_align(lbl_victron_volt, LV_ALIGN_TOP_LEFT, 20, 105);

    lbl_victron_curr = lv_label_create(tab_overview);
    lv_label_set_text(lbl_victron_curr, "Ström: --.- A");
    lv_obj_align(lbl_victron_curr, LV_ALIGN_TOP_LEFT, 20, 125);

    lbl_victron_soc = lv_label_create(tab_overview);
    lv_label_set_text(lbl_victron_soc, "Batterinivå: --- %");
    lv_obj_align(lbl_victron_soc, LV_ALIGN_TOP_LEFT, 20, 145);

    // --- INSTÄLLNINGAR ---
    list_devices = lv_list_create(tab_settings);
    lv_obj_set_size(list_devices, 440, 130);
    lv_obj_align(list_devices, LV_ALIGN_TOP_MID, 0, 10);

    ta_key = lv_textarea_create(tab_settings);
    lv_obj_set_size(ta_key, 440, 40);
    lv_obj_align(ta_key, LV_ALIGN_TOP_MID, 0, 150);
    lv_textarea_set_max_length(ta_key, 32);
    lv_obj_add_flag(ta_key, LV_OBJ_FLAG_HIDDEN);

    btn_set_shunt = lv_btn_create(tab_settings);
    lv_obj_set_size(btn_set_shunt, 140, 35);
    lv_obj_align(btn_set_shunt, LV_ALIGN_TOP_LEFT, 10, 195);
    lv_obj_t * lbl_s = lv_label_create(btn_set_shunt);
    lv_label_set_text(lbl_s, "Sätt som Shunt");
    lv_obj_center(lbl_s);
    lv_obj_add_event_cb(btn_set_shunt, role_shunt_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(btn_set_shunt, LV_OBJ_FLAG_HIDDEN);

    btn_set_mppt = lv_btn_create(tab_settings);
    lv_obj_set_size(btn_set_mppt, 140, 35);
    lv_obj_align(btn_set_mppt, LV_ALIGN_TOP_RIGHT, -10, 195);
    lv_obj_t * lbl_m = lv_label_create(btn_set_mppt);
    lv_label_set_text(lbl_m, "Sätt som MPPT");
    lv_obj_center(lbl_m);
    lv_obj_add_event_cb(btn_set_mppt, role_mppt_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(btn_set_mppt, LV_OBJ_FLAG_HIDDEN);

    kb = lv_keyboard_create(tab_settings);
    lv_obj_set_size(kb, 440, 120);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ta_key);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

void update_gui_data(void) {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (globalData.ruuvi_updated) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Temp: %.2f °C", globalData.ruuvi_temp);
            lv_label_set_text(lbl_ruuvi_temp, buf);
            snprintf(buf, sizeof(buf), "Fukt: %.1f %%", globalData.ruuvi_humidity);
            lv_label_set_text(lbl_ruuvi_hum, buf);
        }

        if (globalData.victron_updated) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Spänning: %.2f V", globalData.victron_voltage);
            lv_label_set_text(lbl_victron_volt, buf);
            snprintf(buf, sizeof(buf), "Ström: %.1f A", globalData.victron_current);
            lv_label_set_text(lbl_victron_curr, buf);
            
            if(globalData.victron_soc > 0.0) {
                snprintf(buf, sizeof(buf), "Batterinivå: %.1f %%", globalData.victron_soc);
                lv_label_set_text(lbl_victron_soc, buf);
            } else {
                lv_label_set_text(lbl_victron_soc, "Batterinivå: -- % (MPPT)");
            }
        }

        if (!globalData.discovered_macs.empty()) {
            for (const auto& mac : globalData.discovered_macs) {
                bool exists = false;
                uint32_t cnt = lv_obj_get_child_cnt(list_devices);
                for(uint32_t i = 0; i < cnt; i++) {
                    lv_obj_t * child = lv_obj_get_child(list_devices, i);
                    const char * text = lv_list_get_btn_text(list_devices, child);
                    if(text && std::string(text).find(mac) != std::string::npos) {
                        exists = true;
                        break;
                    }
                }
                if(!exists) {
                    std::string btn_text = "Victron: " + mac;
                    lv_obj_t * btn = lv_list_add_btn(list_devices, LV_SYMBOL_SETTINGS, btn_text.c_str());
                    lv_obj_add_event_cb(btn, device_click_cb, LV_EVENT_CLICKED, NULL);
                }
            }
            globalData.discovered_macs.clear();
        }
        xSemaphoreGive(dataMutex);
    }
}
