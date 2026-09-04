#pragma once
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// Struktur för att hålla delad sensordata mellan kärnorna
struct SensorData {
    float ruuvi_temp = 0.0;
    float ruuvi_humidity = 0.0;
    float victron_voltage = 0.0;
    float victron_current = 0.0;
    float victron_soc = 0.0;
    bool ruuvi_updated = false;
    bool victron_updated = false;
};

// Globala variabler för trådsäkerhet
extern SensorData globalData;
extern SemaphoreHandle_t dataMutex;

class MyBLEAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // Kontrollera om enheten har tillverkarspecifik data (Manufacturer Data)
        if (advertisedDevice.haveManufacturerData()) {
            std::string manuData = advertisedDevice.getManufacturerData();
            uint16_t companyId = (manuData[1] << 8) | manuData[0];

            // 1. RUUVITAG AVKODNING (Company ID: 0x0499)
            if (companyId == 0x0499 && manuData.length() >= 18) {
                // Kontrollera Dataformat 5 (RAWv2)
                if (manuData[2] == 0x05) {
                    // Temperatur: Två komplement, upplösning 0.005°C
                    int16_t raw_temp = (manuData[3] << 8) | manuData[4];
                    float temp = raw_temp * 0.005;

                    // Luftfuktighet: Osignerad, upplösning 0.0025%
                    uint16_t raw_hum = (manuData[5] << 8) | manuData[6];
                    float hum = raw_hum * 0.0025;

                    // Spara datan trådsäkert med Mutex
                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        globalData.ruuvi_temp = temp;
                        globalData.ruuvi_humidity = hum;
                        globalData.ruuvi_updated = true;
                        xSemaphoreGive(dataMutex);
                    }
                }
            }

            // 2. VICTRON ENERGY AVKODNING (Company ID: 0x02E1)
            else if (companyId == 0x02E1 && manuData.length() >= 8) {
                // Detta är en grundläggande struktur för okrypterad Victron-data (Instant Readout)
                // Beroende på din specifika Victron-modell (SmartShunt/MPPT) ligger värdena på olika bytes.
                // Här läser vi ut ett exempelvärde för spänning (ofta 2 bytes i t.ex. SmartBatterySense)
                uint16_t raw_volt = (manuData[5] << 8) | manuData[4];
                float volt = raw_volt / 100.0; // Exempel: spänning i Volt med 2 decimaler

                if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    globalData.victron_voltage = volt;
                    globalData.victron_updated = true;
                    xSemaphoreGive(dataMutex);
                }
            }
        }
    }
};

// FreeRTOS-uppgift som körs asynkront på Core 0
void bleTask(void *pvParameters) {
    BLEDevice::init("");
    BLEScan* pBLEScan = BLEDevice::getBLEScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyBLEAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    while (true) {
        // Skanna i 4 sekunder, töm sedan cachen för att förhindra minnesläckor
        pBLEScan->start(4, false);
        pBLEScan->clearResults();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Vänta 1 sekund innan nästa skanning
    }
}
