/**
 * =========================================================================
 * Raspberry Pi Pico W — Arduino C++ EMG Sensor Transmitter (USB Serial)
 * =========================================================================
 * 
 * Hardware Wiring:
 *   - EMG Sensor SIG / OUT -> Pico W GP26 (Pin 31 / ADC0)
 *   - EMG Sensor VCC       -> Pico W 3V3(OUT) (Pin 36)
 *   - EMG Sensor GND       -> Pico W AGND / GND (Pin 33 or Pin 38)
 * 
 * Board in Arduino IDE:
 *   - Select: "Raspberry Pi Pico W" (under Raspberry Pi RP2040 Boards)
 * 
 * Instructions:
 *   1. Upload sketch to Pico W.
 *   2. Check COM port in Arduino IDE (e.g. COM4).
 *   3. In pc/config.json, set:
 *        "emg_source": "serial",
 *        "serial_port": "COM4",
 *        "serial_baud_rate": 115200
 */

const int EMG_PIN = 26; // GP26 / ADC0
const unsigned long SAMPLE_INTERVAL_US = 1000; // 1000 Hz

unsigned long next_sample_us = 0;

void setup() {
    Serial.begin(115200);
    analogReadResolution(12); // 12-bit ADC (0 - 4095)
    pinMode(EMG_PIN, INPUT);
    next_sample_us = micros();
}

void loop() {
    unsigned long now = micros();
    if (now - next_sample_us >= SAMPLE_INTERVAL_US) {
        next_sample_us += SAMPLE_INTERVAL_US;

        int raw_value = analogRead(EMG_PIN); // 0 - 4095
        Serial.println(raw_value);
    }
}
