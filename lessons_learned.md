# 📑 Lessons Learned - Waveshare ESP32-S3-Touch-LCD-4 v4 Development

Detta dokument sammanställer de tekniska upptäckter, dolda hårdvarufällor och optimeringsstrategier som identifierades under utvecklingen baserat på Waveshares officiella källkodsexempel.

---

## 1. 🔌 RS485 & Modbus RTU (`13_RS485`)

### Insikter & Hårdvaruarkitektur
* **Pin-Mappning:** Hårdvarutransceivern (SP3485) är permanent ansluten till **GPIO 43 (RX)** och **GPIO 44 (TX)** [13_RS485]. Den måste styras via en dedikerad hårdvaru-UART (`HardwareSerial Serial1`) [13_RS485].
* **Automatisk Flödeskontroll:** Kretskortet är designat med en hårdvarubaserad automatisk riktningsväxlare (auto-direction) [13_RS485]. Mjukvaran behöver *inte* manuellt dra en DE/RE-pinne hög eller låg före/efter sändning [13_RS485].
* **Blockerande Transmission:** Eftersom UART-sändningar sker i bakgrunden via skiftregister, måste `RS485Serial.flush()` anropas omedelbart efter ett Modbus-kommando. Detta säkerställer att hela paketet har lämnat kretskortet innan processorn återgår till tunga grafikberäkningar.

---

## 📡 2. CAN Bus / TWAI (`12_TWAIreceive`)

### Insikter & Hårdvaruarkitektur
* **Pin-Mappning:** CAN-transceivern (TJA1051) är kopplad till **GPIO 41 (RX)** och **GPIO 42 (TX)** [12_TWAIreceive].
* **Protokollramverk:** ESP32-S3 har ingen "hårdvaru-CAN" i traditionell mening, utan använder Espressifs egna **TWAI** (Two-Wire Automotive Interface)-drivrutin [12_TWAIreceive].
* **Buffertmättnad:** TWAI-mottagningen är strikt händelsebaserad via interna ringbuffertar [12_TWAIreceive]. Om `twai_receive()` inte pollas kontinuerligt i mikrosekundsloopar (non-blocking, timeout=0) blir bufferten full, vilket leder till tysta paketförluster utan felmeddelanden [12_TWAIreceive].

---

## 🛠️ 3. Den Dolda Fällan: I2C I/O-Expandern (CH422G / PCA9554)

### Den Största Hårdvaruutmaningen
* **Problemet:** Vid initiala tester förblev både RS485- och CAN-bussarna helt döda, trots korrekt initierade UART- och TWAI-drivrutiner.
* **Orsak funnen i schemat:** Waveshare har placerat en **I2C-styrd I/O-expander** på sensorbussen (**GPIO 11 (SDA)** och **GPIO 12 (SCL)**). Denna expander fungerar som en elektronisk strömbrytare (Isolation Power Control) för bussarnas transceivers.
* **Lösning:** Innan bussarna används måste mjukvaran initiera `Wire` och skicka en specifik registersekvens till expanderns I2C-adress (`0x20`) för att sätta pinnarna till utgångar och dra dem höga. Görs inte detta förblir kringutrustningen strömlös.

---

## 🖥️ 4. Display, Touch & Minnesallokering (`09_LVGL_Widgets`)

### Insikter & Hårdvaruarkitektur
* **PSRAM är ett krav:** Skärmen (ST7701) har en upplösning på 480x480 pixlar och körs över ett 16-bitars RGB-gränssnitt [09_LVGL_Widgets]. Detta kräver gigantiska frame-buffers (våra djupt integrerade dubbel-buffers för att förhindra skärmflimmer) som helt enkelt inte ryms i ESP32-S3:s interna SRAM. 
* **Arduino IDE Inställning:** Kompileringen misslyckas eller kraschar i en boot-loop om inte **PSRAM: OPI PSRAM** är aktiverat i utvecklingsmiljön.
* **LVGL Versionstolerans:** Waveshares BSP (Board Support Package) är strikt låst och validerat mot **LVGL v8.3.x/v8.4.0** [09_LVGL_Widgets]. Att försöka migrera till LVGL v9 i Arduino IDE 2.3.10 bryter bakomliggande drivrutinsanrop och renderingstimer.

---

## ⏰ 5. Offline-tidshållning (`05_GFX_PCF85063_simpleTime`)

### Insikter & Hårdvaruarkitektur
* **Hårdvaru-RTC:** Den inbyggda klockan är en **PCF85063** på I2C-adress `0x51` [05_GFX_PCF85063_simpleTime].
* **BCD-Format:** Chippet kommunicerar enbart i **BCD (Binary-Coded Decimal)** [05_GFX_PCF85063_simpleTime]. Vid läsning måste bit-skiftning utföras (`(val/16*10) + (val%16)`) för att översätta värdet till Arduinos decimala heltal, och vice versa vid skrivning från webbgränssnittets manuella klockinställning [05_GFX_PCF85063_simpleTime].

---

## 🚀 6. Slutgiltig Mjukvaruoptimering (Core Separation)

För att garantera dygnet-runt-drift utan hängningar implementerades följande arkitekturregler:

1. **Kärn-Asynkronism (FreeRTOS):** Radiotrafik (WiFi/BLE) placerades på **Core 0**, medan grafik (LVGL) och Modbus flyttades till **Core 1**. Detta förhindrar att en tung BLE-skanning eller en seg webbklient fryser touch-skärmens rendering eller touch-mottagning [09_LVGL_Widgets].
2. **Trådsäkra Mutexar:** Delat minne skyddas stenhårt med `SemaphoreHandle_t` Mutex-lås. En kärna tillåts aldrig manipulera enhetsarrayer utan att först ha tilldelats låset.
3. **Stoppa Heap-fragmentering:** Inga dynamiska `String`-objekt tillåts i looparna. All strängformatering sker via förallokerade statiska `char`-buffertar och `snprintf()`, vilket säkrar en stabil drifttid över flera månader.
