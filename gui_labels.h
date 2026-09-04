#pragma once
#include <lvgl.h>
#include <map>
#include <string>
#include "ble_processor.h"

// Externa variabler deklarerade i huvudfilen
extern SensorData globalData;
extern SemaphoreHandle_t dataMutex;
extern std::map<std::string, std::string> victronKeys; // MAC -> Bindkey

// LVGL UI-element
static lv_obj_t *tabview;
static lv_obj_t *tab_overview;
static lv_obj_t *tab_settings;

// Grafiska etiketter för realtidsdata
static lv_obj_t *lbl_ruuvi_temp;
static lv_obj_t *lbl_ruuvi_hum;
static lv_obj_t *lbl_victron_mac;
static lv_obj_t *lbl_victron_volt;

// Inställningskomponenter
static lv_obj_t *list_devices;
static lv_obj_t *kb;
static lv_obj_t *ta_key;
static std::string selected_mac = "";

// Tangentbordshändelse - Sparar bindkey till vald enhet
static void kb_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if(code == LV_EVENT_READY && !selected_mac.empty()) {
            const char * key_str = lv_textarea_get_text(ta_key);
            if(strlen(key_str) == 32) { // En giltig Victron Bindkey är alltid 32 tecken
                if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    victronKeys[selected_mac] = std::string(key_str);
                    xSemaphoreGive(dataMutex);
                }
            }
        }
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ta_key, LV_OBJ_FLAG_HIDDEN);
    }
}

// När man klickar på en funnen Victron-enhet i listan
static void device_click_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    selected_mac = lv_list_get_btn_text(list_devices, btn);
    
    // Ta bort "Victron: " prefixet från strängen för att få ren MAC
    size_t pos = selected_mac.find("0x");
    if(pos == std::string::npos) pos = selected_mac.find(" ");
    if(pos != std::string::npos) selected_mac = selected_mac.substr(pos + 1);

    lv_textarea_set_text(ta_key, "");
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ta_key, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_placeholder_text(ta_key, "Mata in 32-tecken Bindkey...");
}

// Initiera gränssnittet och flikarna
void init_gui(void) {
    // Skapa flikvy (Tab View) placerad i toppen av skärmen
    tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 50);

    tab_overview = lv_tabview_add_tab(tabview, "📊 Oversikt");
    tab_settings = lv_tabview_add_tab(tabview, "⚙️ Inställningar");

    // --- BYGG FLIK 1: ÖVERSIKT ---
    lv_obj_t * title_ruuvi = lv_label_create(tab_overview);
    lv_label_set_text(title_ruuvi, "🌿 RuuviTag Sensor");
    lv_obj_align(title_ruuvi, LV_ALIGN_TOP_LEFT, 10, 10);

    lbl_ruuvi_temp = lv_label_create(tab_overview);
    lv_label_set_text(lbl_ruuvi_temp, "Temp: --.- °C");
    lv_obj_align(lbl_ruuvi_temp, LV_ALIGN_TOP_LEFT, 20, 40);

    lbl_ruuvi_hum = lv_label_create(tab_overview);
    lv_label_set_text(lbl_ruuvi_hum, "Fukt: --.- %");
    lv_obj_align(lbl_ruuvi_hum, LV_ALIGN_TOP_LEFT, 20, 60);

    lv_obj_t * divider = lv_obj_create(tab_overview);
    lv_obj_set_size(divider, 440, 2);
    lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t * title_victron = lv_label_create(tab_overview);
    lv_label_set_text(title_victron, "⚡ Victron Energy (Aktiv)");
    lv_obj_align(title_victron, LV_ALIGN_TOP_LEFT, 10, 120);

    lbl_victron_mac = lv_label_create(tab_overview);
    lv_label_set_text(lbl_victron_mac, "Enhet: Ingen parad");
    lv_obj_align(lbl_victron_mac, LV_ALIGN_TOP_LEFT, 20, 150);

    lbl_victron_volt = lv_label_create(tab_overview);
    lv_label_set_text(lbl_victron_volt, "Spänning: --.-- V");
    lv_obj_align(lbl_victron_volt, LV_ALIGN_TOP_LEFT, 20, 180);


    // --- BYGG FLIK 2: INSTÄLLNINGAR ---
    lv_obj_t * settings_title = lv_label_create(tab_settings);
    lv_label_set_text(settings_title, "Upptäckta Victron-enheter (Klicka för nyckel):");
    lv_obj_align(settings_title, LV_ALIGN_TOP_LEFT, 10, 10);

    // Lista över upptäckta enheter
    list_devices = lv_list_create(tab_settings);
    lv_obj_set_size(list_devices, 440, 180);
    lv_obj_align(list_devices, LV_ALIGN_TOP_MID, 0, 40);

    // Textruta för nyckel (gömd som standard)
    ta_key = lv_textarea_create(tab_settings);
    lv_obj_set_size(ta_key, 440, 40);
    lv_obj_align(ta_key, LV_ALIGN_BOTTOM_MID, 0, -160);
    lv_textarea_set_max_length(ta_key, 32);
    lv_obj_add_flag(ta_key, LV_OBJ_FLAG_HIDDEN);

    // Virtuellt tangentbord (gömd som standard)
    kb = lv_keyboard_create(tab_settings);
    lv_obj_set_size(kb, 440, 140);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ta_key);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

// Uppdatera värden på skärmen (Kallas från loop() på Core 1)
void update_gui_data(void) {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        // Uppdatera Ruuvi-etiketter
        if (globalData.ruuvi_updated) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Temp: %.2f °C", globalData.ruuvi_temp);
            lv_label_set_text(lbl_ruuvi_temp, buf);
            
            snprintf(buf, sizeof(buf), "Fukt: %.1f %%", globalData.ruuvi_humidity);
            lv_label_set_text(lbl_ruuvi_hum, buf);
        }

        // Uppdatera Victron-etiketter
        if (globalData.victron_updated) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Enhet: %s", globalData.active_victron_mac.c_str());
            lv_label_set_text(lbl_victron_mac, buf);

            snprintf(buf, sizeof(buf), "Spänning: %.2f V", globalData.victron_voltage);
            lv_label_set_text(lbl_victron_volt, buf);
        }

        // Lägg dynamiskt till nya upptäckta enheter i listan under Inställningar
        if (!globalData.discovered_macs.empty()) {
            for (const auto& mac : globalData.discovered_macs) {
                // Kontrollera om knappen redan finns i listan för att förhindra dubbletter
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
            globalData.discovered_macs.clear(); // Töm kön efter parning till listan
        }
        xSemaphoreGive(dataMutex);
    }
}
