/**
 * =========================================================================
 * Analog EMG Sensor Transmitter (Arduino / Microcontroller Sketch)
 * =========================================================================
 * 
 * Use this sketch if your EMG sensor (MyoWare, AD8232, SEN0240, Gravity, etc.)
 * is connected to an Arduino Uno / Nano / ESP32 ADC pin and streams to the PC.
 * 
 * Hardware Wiring:
 *   - Arduino Uno / Nano : EMG SIG / OUT -> Pin A0
 *   - ESP32              : EMG SIG / OUT -> GPIO 36 (VP / ADC1_CH0) or GPIO 34
 *   - Raspberry Pi Pico  : EMG SIG / OUT -> GP26 (Pin 31 / ADC0)
 *   - VCC                : 5V (Arduino) or 3.3V (ESP32 / Pico / 3.3V sensors)
 *   - GND                : Common GND
 * 
 * Electrode Placement:
 *   - RED / IN+    : Center of the target muscle belly (e.g. forearm flexor)
 *   - BLUE / IN-   : Along the length of the same muscle belly (2-3 cm away)
 *   - BLACK / REF  : Bony prominence with no muscle activity (elbow or wrist)
 * 
 * Instructions:
 *   1. Upload this sketch to your Arduino/board.
 *   2. Note the COM port in Arduino IDE (e.g. COM3).
 *   3. In d:\EMG\pc\config.json, set:
 *        "emg_source": "serial",
 *        "serial_port": "COM3",
 *        "serial_baud_rate": 115200
 */
#if defined(ARDUINO) || __has_include(<Arduino.h>)
#include <Arduino.h>
#else
// Fallback declarations for IDE language servers (e.g. clangd) analyzing .ino files without Arduino core
#ifndef A0
#define A0 14
#endif
#ifndef INPUT
#define INPUT 0
#endif
#ifndef OUTPUT
#define OUTPUT 1
#endif
void pinMode(int pin, int mode);
int analogRead(int pin);
unsigned long micros(void);
struct HardwareSerialStub {
    void begin(unsigned long baud) {}
    void print(int val) {}
    void print(const char* s) {}
    void println(int val) {}
    void println(const char* s = "") {}
};
static HardwareSerialStub Serial;
#endif

// Define EMG analog input pin based on target microcontroller:
#if defined(ESP32)
// ESP32: GPIO 36 (VP) uses ADC1 (safe with Wi-Fi enabled)
const int EMG_PIN = 36;
#elif defined(ARDUINO_ARCH_RP2040)
// Raspberry Pi Pico / RP2040: GP26 is ADC0
const int EMG_PIN = 26;
#else
// Standard Arduino Uno / Nano / Mega (AVR) or IDE fallback
const int EMG_PIN = A0;
#endif
const unsigned long SAMPLE_INTERVAL_MICROS = 1000; // 1000 Hz = 1000 microseconds

unsigned long last_sample_time = 0;

void setup() {
    Serial.begin(115200);
    pinMode(EMG_PIN, INPUT);
}

void loop() {
    unsigned long current_time = micros();

    if (current_time - last_sample_time >= SAMPLE_INTERVAL_MICROS) {
        last_sample_time = current_time;

        int raw_value = analogRead(EMG_PIN); // 0 - 1023
        Serial.println(raw_value);
    }
}
