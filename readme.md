# Victron & RuuviTag BLE Touch Display

Detta projekt är en applikation för **Waveshare ESP32-S3-Touch-LCD-4 (Version 4)** [en-lessons_learned.md]. Den samlar in, avkodar och visar realtidsdata från **RuuviTag** (temperatur/luftfuktighet) samt **Victron Energy BLE-enheter** (SmartShunt och SmartSolar MPPT) [esphome-victron_ble] via Bluetooth Low Energy (BLE).

Projektet är utvecklat och testat i **Arduino IDE 2.3.10**.

## 🚀 Funktioner
* **Multi-core Arkitektur:** All BLE-skanning, filtrering och AES-dekryptering körs asynkront på **Core 0** [en-lessons_learned.md]. Det grafiska gränssnittet (LVGL) drivs ostört på **Core 1** vilket förhindrar lagg vid touch-inmatning [09_LVGL_Widgets, en-lessons_learned.md].
* **Victron AES-128 CTR-dekryptering:** Hårdvaruaccelererad dekryptering i realtid av Victrons krypterade *Instant Readout*-paket via ESP32:s inbyggda `mbedtls`-bibliotek.
* **Smart Enhetsväljare (GUI):** Inbyggt fliksystem (Tabs) i LVGL v8 [en-lessons_learned.md]. Under fliken **Inställningar** listas alla upptäckta Victron-enheter. Du kan mata in dess 32-teckens *Bindkey* via ett virtuellt tangentbord samt tilldela enheten rollen som antingen *SmartShunt* eller *MPPT*.
* **Permanent Lagring:** Alla inmatade Bindkeys samt enhetsroller sparas permanent i ESP32:s flashminnet via `Preferences` och laddas automatiskt vid uppstart.
* **Optimerad Sökfrekvens:** BLE-skannern söker i 3 sekunder och vilar sedan i 10 sekunder för att spara processorresurser och minimera radiostörningar [en-lessons_learned.md].

## 📦 Projektstruktur
Projektet består av följande tre filer i källkodsmappen:
1. `Victron_Ruuvi_Touch.ino` – Huvudfilen som initierar displayen (RGB-panelen), touch-kontrollern (GT911), startar FreeRTOS-uppgifter och laddar inställningar från flashminnet vid boot [09_LVGL_Widgets, en-lessons_learned.md].
2. `ble_processor.h` – Hanterar BLE-skanningen, filtrerar tillverkardata (Company ID `0x0499` för Ruuvi och `0x02E1` för Victron) samt utför dekryptering och byte-packning [esphome-victron_ble].
3. `gui_labels.h` – Bygger användargränssnittet i LVGL v8 och hanterar alla interaktioner som tangentbordsinmatningar och listor [09_LVGL_Widgets, en-lessons_learned.md].

## 🛠️ Inställningar i Arduino IDE 2.3.10
För att koden ska kompilera och fungera på Waveshare-hårdvaran måste följande kortinställningar väljas under menyn **Verktyg (Tools)**:

* **Board:** `ESP32S3 Dev Module`
* **PSRAM:** **`OPI PSRAM`** *(Helt kritiskt – skärmens grafikbuffertar kräver detta för att inte hamna i boot-loop)* [en-lessons_learned.md]
* **Core Debug Level:** `None` eller `Error`

### Nödvändiga bibliotek (installeras via Library Manager)
* `LVGL` (Verifierat mot **v8.3.x / v8.4.0** – *Använd inte v9 då det bryter Waveshares skärmdrivrutiner*) [en-lessons_learned.md]
* `Arduino_GFX_Library` [09_LVGL_Widgets]

## 📖 Användarinstruktioner
1. Starta displayen och navigera till fliken **⚙️ Inställningar**.
2. När din Victron-enhet (SmartShunt eller MPPT) dyker upp i listan över tillgängliga enheter, klicka på den.
3. Ett virtuellt tangentbord öppnas. Skriv in din **32-tecken långa Bindkey** (AES-krypteringsnyckel) som du hämtat från VictronConnect-appen och tryck på bock-symbolen.
4. Klicka på enheten en gång till och välj antingen **"Sätt som Shunt"** eller **"Sätt som MPPT"** beroende på hårdvara.
5. Gå tillbaka till fliken **📊 Översikt** för att se ditt systems realtidsdata!
