#pragma once
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <map>
#include <string>
#include <algorithm>
#include "mbedtls/aes.h" // Inbyggt hårdvaruaccelererat krypto-bibliotek i ESP32

// Struktur för att hålla delad sensordata mellan kärnorna
struct SensorData {
    float ruuvi_temp = 0.0;
    float ruuvi_humidity = 0.0;
    float victron_voltage = 0.0;
    float victron_current = 0.0;
    float victron_soc = 0.0;
    bool ruuvi_updated = false;
    bool victron_updated = false;
    std::string active_victron_mac = "";
    std::vector<std::string> discovered_macs;
};

// Globala trådsäkra variabler (allokerade i huvudfilen)
extern SensorData globalData;
extern SemaphoreHandle_t dataMutex;
extern std::map<std::string, std::string> victronKeys; // MAC -> Bindkey (Sparade i Flash)

// Hjälpfunktion: Konverterar en hex-sträng (Bindkey) till råa bytes
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
            if (manuData.length() < 4) return;

            // Extrahera Company ID (De första 2 byten)
            uint16_t companyId = (uint8_t)manuData[1] << 8 | (uint8_t)manuData[0];
            std::string macAddress = advertisedDevice.getAddress().toString();

            // -------------------------------------------------------
            // 1. RUUVITAG AVKODNING (Company ID: 0x0499)
            // -------------------------------------------------------
            if (companyId == 0x0499 && manuData.length() >= 18) {
                if ((uint8_t)manuData[2] == 0x05) { // Dataformat 5 (RAWv2)
                    int16_t raw_temp = ((uint8_t)manuData[3] << 8) | (uint8_t)manuData[4];
                    uint16_t raw_hum = ((uint8_t)manuData[5] << 8) | (uint8_t)manuData[6];
                    
                    float temp = raw_temp * 0.005;
                    float hum = raw_hum * 0.0025;

                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        globalData.ruuvi_temp = temp;
                        globalData.ruuvi_humidity = hum;
                        globalData.ruuvi_updated = true;
                        xSemaphoreGive(dataMutex);
                    }
                }
            }

            // -------------------------------------------------------
            // 2. VICTRON ENERGY DEKRYPTERING (Company ID: 0x02E1)
            // -------------------------------------------------------
            else if (companyId == 0x02E1 && manuData.length() >= 12) {
                std::string currentKey = "";
                
                // Kontrollera trådsäkert om vi har en sparad Bindkey för denna MAC
                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    // Lägg till i listan över upptäckta enheter om den inte redan finns där
                    if (std::find(globalData.discovered_macs.begin(), globalData.discovered_macs.end(), macAddress) == globalData.discovered_macs.end()) {
                        globalData.discovered_macs.push_back(macAddress);
                    }
                    
                    if (victronKeys.find(macAddress) != victronKeys.end()) {
                        currentKey = victronKeys[macAddress];
                    }
                    xSemaphoreGive(dataMutex);
                }

                // Om vi har en sparad nyckel till denna enhet – påbörja AES-128 CTR
                if (currentKey.length() == 32) {
                    uint8_t keyBytes[16];
                    if (!hexStringToBytes(currentKey, keyBytes, 16)) return;

                    // Victron-paketet är uppbyggt så här:
                    // manuData[2] = Record Type
                    // manuData[3..4] = Model ID
                    // manuData[5..6] = Nonce / Krypteringsräknare (Little Endian!)
                    uint16_t nonce = ((uint8_t)manuData[6] << 8) | (uint8_t)manuData[7]; 

                    // Skapa den 16 byte långa IV (Initialization Vector) för AES-CTR mode
                    uint8_t iv[16];
                    memset(iv, 0, 16);
                    iv[0] = (uint8_t)manuData[5]; // Byte 5 (Lägsta delen av noncet)
                    iv[1] = (uint8_t)manuData[6]; // Byte 6 (Högsta delen av noncet)
                    // Resten av IV lämnas som nollor enligt Victrons specifikation

                    // Extrahera den krypterade nyttolasten (börjar vid byte index 7 eller 8 beroende på modell)
                    size_t payloadLen = manuData.length() - 8;
                    uint8_t ciphertext[32];
                    uint8_t decrypted[32];
                    
                    if(payloadLen > 32) payloadLen = 32; // Säkerhetsspärr för buffer overflow
                    
                    for(size_t i = 0; i < payloadLen; i++) {
                        ciphertext[i] = (uint8_t)manuData[8 + i];
                    }

                    // Initiera mbedTLS AES-kontexten
                    mbedtls_aes_context aes;
                    mbedtls_aes_init(&aes);
                    mbedtls_aes_setkey_enc(&aes, keyBytes, 128); // 128-bitars kryptering

                    size_t nc_off = 0;
                    uint8_t stream_block[16];
                    memset(stream_block, 0, 16);

                    // Utför själva AES-128 CTR-dekrypteringen
                    int ret = mbedtls_aes_crypt_ctr(&aes, payloadLen, &nc_off, iv, stream_block, ciphertext, decrypted);
                    mbedtls_aes_free(&aes);

                    if (ret == 0) { // 0 betyder lyckad dekryptering!
                        // Tolka den dekrypterade datan baserat på Victron-standard
                        // För de flesta enheter (t.ex. SmartShunt/Battery Sense):
                        // decrypted[0..1] = Spänning (skalat med 100 eller 10, d.v.s. 0.01V)
                        uint16_t raw_volt = (decrypted[1] << 8) | decrypted[0];
                        float voltage = raw_volt / 100.0; 

                        // Spara datan till Core 1-gränssnittet
                        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                            globalData.victron_voltage = voltage;
                            globalData.active_victron_mac = macAddress;
                            globalData.victron_updated = true;
                            xSemaphoreGive(dataMutex);
                        }
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
        pBLEScan->start(4, false);
        pBLEScan->clearResults();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
