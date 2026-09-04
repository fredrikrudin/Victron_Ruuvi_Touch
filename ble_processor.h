#pragma once
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include "mbedtls/aes.h"

struct SensorData {
    float ruuvi_temp = 0.0;
    float ruuvi_humidity = 0.0;
    
    // Victron samlad data
    float victron_voltage = 0.0;
    float victron_current = 0.0;
    float victron_soc = 0.0;
    
    bool ruuvi_updated = false;
    bool victron_updated = false;
    
    std::vector<std::string> discovered_macs;
    std::string shunt_mac = "";
    std::string mppt_mac = "";
};

extern SensorData globalData;
extern SemaphoreHandle_t dataMutex;
extern std::map<std::string, std::string> victronKeys;

bool hexStringToBytes(const std::string& hex, uint8_t* bytes, size_t maxLen) {
    if (hex.length() != maxLen * 2) return false;
    for (size_t i = 0; i < maxLen; i++) {
        std::string byteString = hex.substr(i * 2, 2);
        bytes[i] = (uint8_t) strtol(byteString.c_str(), NULL, 16);
    }
    return true;
}

class MyBLEAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.haveManufacturerData()) {
            std::string manuData = advertisedDevice.getManufacturerData();
            if (manuData.length() < 12) return;

            uint16_t companyId = (uint8_t)manuData[1] << 8 | (uint8_t)manuData[0];
            std::string macAddress = advertisedDevice.getAddress().toString();

            // 1. RUUVITAG
            if (companyId == 0x0499 && manuData.length() >= 18) {
                if ((uint8_t)manuData[2] == 0x05) {
                        int16_t raw_temp = ((uint8_t)manuData[3] << 8) | (uint8_t)manuData[4];
                        uint16_t raw_hum = ((uint8_t)manuData[5] << 8) | (uint8_t)manuData[6];
                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        globalData.ruuvi_temp = raw_temp * 0.005;
                        globalData.ruuvi_humidity = raw_hum * 0.0025;
                        globalData.ruuvi_updated = true;
                        xSemaphoreGive(dataMutex);
                    }
                }
            }
            // 2. VICTRON ENERGY
            else if (companyId == 0x02E1) {
                std::string currentKey = "";
                bool isShunt = false;
                bool isMppt = false;

                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    if (std::find(globalData.discovered_macs.begin(), globalData.discovered_macs.end(), macAddress) == globalData.discovered_macs.end()) {
                        globalData.discovered_macs.push_back(macAddress);
                    }
                    if (victronKeys.find(macAddress) != victronKeys.end()) {
                        currentKey = victronKeys[macAddress];
                    }
                    isShunt = (macAddress == globalData.shunt_mac);
                    isMppt = (macAddress == globalData.mppt_mac);
                    xSemaphoreGive(dataMutex);
                }

                if (currentKey.length() == 32 && (isShunt || isMppt)) {
                    uint8_t keyBytes[16];
                    if (!hexStringToBytes(currentKey, keyBytes, 16)) return;

                    uint16_t nonce = ((uint8_t)manuData[6] << 8) | (uint8_t)manuData[5];
                    uint8_t iv[16];
                    memset(iv, 0, 16);
                    iv[0] = (uint8_t)manuData[5];
                    iv[1] = (uint8_t)manuData[6];

                    size_t payloadLen = manuData.length() - 8;
                    uint8_t ciphertext[32];
                    uint8_t decrypted[32];
                    if(payloadLen > 32) payloadLen = 32;

                    for(size_t i = 0; i < payloadLen; i++) {
                        ciphertext[i] = (uint8_t)manuData[8 + i];
                    }

                    mbedtls_aes_context aes;
                    mbedtls_aes_init(&aes);
                    mbedtls_aes_setkey_enc(&aes, keyBytes, 128);
                    size_t nc_off = 0;
                    uint8_t stream_block[16];
                    memset(stream_block, 0, 16);

                    if (mbedtls_aes_crypt_ctr(&aes, payloadLen, &nc_off, iv, stream_block, ciphertext, decrypted) == 0) {
                        mbedtls_aes_free(&aes);

                        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                            if (isShunt) {
                                // SmartShunt/BMV byte-mappning [esphome-victron_ble]:
                                // Byte 0-1: Spänning (skalat med 100, dvs 0.01V per enhet)
                                // Byte 2-3: Ström (skalat med 10, dvs 0.1A per enhet)
                                // Byte 4-5: SoC (skalat med 10, dvs 0.1% per enhet)
                                uint16_t raw_v = (decrypted[1] << 8) | decrypted[0];
                                int16_t raw_c = (decrypted[3] << 8) | decrypted[2];
                                uint16_t raw_soc = (decrypted[5] << 8) | decrypted[4];

                                globalData.victron_voltage = raw_v / 100.0;
                                globalData.victron_current = raw_c / 10.0;
                                globalData.victron_soc = (raw_soc & 0x3FF) / 10.0; // Victron maskar ofta SoC bitar
                                globalData.victron_updated = true;
                            } 
                            else if (isMppt) {
                                // SmartSolar MPPT byte-mappning [esphome-victron_ble]:
                                // Byte 0-1: Batterispänning (0.01V)
                                // Byte 2-3: Laddström till batteri (0.1A)
                                uint16_t raw_v = (decrypted[1] << 8) | decrypted[0];
                                uint16_t raw_c = (decrypted[3] << 8) | decrypted[2];

                                globalData.victron_voltage = raw_v / 100.0;
                                globalData.victron_current = raw_c / 10.0;
                                // MPPT skickar inte SoC, så vi rör inte den sensorn här
                                globalData.victron_updated = true;
                            }
                            xSemaphoreGive(dataMutex);
                        }
                    } else {
                        mbedtls_aes_free(&aes);
                    }
                }
            }
        }
    }
};

void bleTask(void *pvParameters) {
    BLEDevice::init("");
    BLEScan* pBLEScan = BLEDevice::getBLEScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyBLEAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(150);
    pBLEScan->setWindow(140);

    while (true) {
        pBLEScan->start(3, false); // Skanna i 3 sekunder
        pBLEScan->clearResults();
        vTaskDelay(pdMS_TO_TICKS(10000)); // HÄR ÄR ÄNDRINGEN: Vila i 10 sekunder innan nästa sökning
    }
}
