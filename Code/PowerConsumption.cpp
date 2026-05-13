#include <Arduino.h>
#include <heltec_unofficial.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "arduinoFFT.h"

// --- Configuration ---
#define SAMPLING_FREQ 100.0   
#define SAMPLES 128           
#define WINDOW_MS 5000        
#define REST_MS 5000          

// Global Variables
double vReal[SAMPLES], vImag[SAMPLES];
float currentMa = 0, busVoltage = 0, powerMw = 0;
float detectedPeak = 0;
bool isResting = false;

Adafruit_INA219 ina219;
ArduinoFFT<double> FFT;

// --- SAMPLER (CORE 0) ---
void samplerTask(void *pv) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000 / SAMPLING_FREQ); 

    while (true) {
        if (!isResting) {
            for (int i = 0; i < SAMPLES; i++) {
                float t = micros() / 1000000.0f;
                vReal[i] = 2.0 * sin(2.0 * PI * 3.0 * t) + 4.0 * sin(2.0 * PI * 5.0 * t);
                vImag[i] = 0;

                currentMa = ina219.getCurrent_mA();
                busVoltage = ina219.getBusVoltage_V();
                powerMw = busVoltage * currentMa;

                vTaskDelayUntil(&xLastWakeTime, xFrequency);
            }

            FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);
            FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
            FFT.compute(FFT_FORWARD);
            FFT.complexToMagnitude();
            detectedPeak = (float)FFT.majorPeak();
        } else {
            detectedPeak = 0; // Reset peak while resting
            currentMa = ina219.getCurrent_mA();
            busVoltage = ina219.getBusVoltage_V();
            powerMw = busVoltage * currentMa;
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
    }
}

// --- CONTROLLER (CORE 1) ---
void controllerTask(void *pv) {
    while (true) {
        isResting = false;
        display.setContrast(255); 
        vTaskDelay(pdMS_TO_TICKS(WINDOW_MS));

        isResting = true;
        display.setContrast(0);   
        vTaskDelay(pdMS_TO_TICKS(REST_MS));
    }
}

void setup() {
    heltec_setup();
    Serial.begin(115200);

    Wire1.begin(19, 20);
    ina219.begin(&Wire1);

    xTaskCreatePinnedToCore(samplerTask, "Sampler", 8192, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(controllerTask, "Controller", 4096, NULL, 5, NULL, 1);
}

void loop() {
    // --- TELEPLOT OUTPUT ---
    static uint32_t lastTele = 0;
    if (millis() - lastTele > 50) { 
        Serial.printf(">Voltage_V:%.2f\n", busVoltage);
        Serial.printf(">Current_mA:%.2f\n", currentMa);
        Serial.printf(">Power_mW:%.2f\n", powerMw);
        // Force peak to 0 in teleplot if resting
        Serial.printf(">Detected_Peak_Hz:%.2f\n", isResting ? 0.0f : detectedPeak);
        lastTele = millis();
    }

    // --- OLED DISPLAY ---
    static uint32_t lastDisp = 0;
    if (millis() - lastDisp > 200) {
        display.clear();
        if (!isResting) {
            display.drawString(0, 0, "MODE: ACTIVE (100Hz)");
            display.drawString(0, 12, "Peak: " + String(detectedPeak, 1) + " Hz");
        } else {
            display.drawString(0, 0, "MODE: RESTING");
            display.drawString(0, 12, "(Screen Dimmed)");
        }
        
        display.drawString(0, 28, "V: " + String(busVoltage, 2) + " V");
        display.drawString(0, 40, "P: " + String(powerMw, 1) + " mW");
        
        display.display();
        lastDisp = millis();
    }
}
