/**
 * =========================================================================
 * Analog EMG Sensor Transmitter (Arduino / Microcontroller Sketch)
 * =========================================================================
 * 
 * Use this sketch if your EMG sensor (MyoWare, AD8232, SEN0240, Gravity, etc.)
 * is connected to an Arduino Uno / Nano / ESP32 ADC pin and streams to the PC.
 * 
 * Hardware Wiring:
 *   - EMG Sensor SIG / OUT -> Arduino Pin A0
 *   - EMG Sensor VCC       -> Arduino 5V (or 3.3V depending on sensor)
 *   - EMG Sensor GND       -> Arduino GND
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

const int EMG_PIN = A0;
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
