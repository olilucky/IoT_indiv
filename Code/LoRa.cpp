#include <Arduino.h>
#include <heltec_unofficial.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "arduinoFFT.h"

// --- TTN Credentials ---
// Ensure these match your TTN Console exactly!
uint64_t devEui = 0x70B3D57ED007778DULL;
uint64_t joinEui = 0x0000000000000000ULL;
uint8_t appKey[] = { 0x43, 0x09, 0x8A, 0xFC, 0x15, 0xD7, 0xE7, 0xA8, 0xB1, 0x8E, 0xBE, 0x1B, 0x51, 0xBF, 0xA2, 0xEC };

LoRaWANNode node(&radio, &EU868);

// Global Variables
double vReal[128], vImag[128];
volatile double peakFreq = 0.0;
float currentMa = 0;
volatile bool phaseComplete = false; 
Adafruit_INA219 ina219;
ArduinoFFT<double> FFT; 

void SamplerTask(void * pvParameters) {
    while(true) {
        unsigned long microseconds;
        double samplingFreq = 40.0; 
        double sampling_period_us = 1000000.0 / samplingFreq;
        for (int i = 0; i < 128; i++) {
            microseconds = micros();
            vReal[i] = 2.0 * sin(2.0 * PI * 3.0 * (micros()/1000000.0)) + 4.0 * sin(2.0 * PI * 5.0 * (micros()/1000000.0));
            vImag[i] = 0;
            while (micros() - microseconds < sampling_period_us) { yield(); }
        }
        FFT = ArduinoFFT<double>(vReal, vImag, 128, samplingFreq);
        FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        FFT.compute(FFT_FORWARD);
        FFT.complexToMagnitude();
        double temp = FFT.majorPeak();
        if (temp > 0) peakFreq = temp;
        phaseComplete = true; 
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    heltec_setup();
    heltec_ve(true);
    
    display.setContrast(255);
    Serial.println("\n[DEBUG] Powering up...");

    // Set Sync Word (0x34 is for public LoRaWAN like TTN)
    radio.setSyncWord(0x34); 
    
    Serial.println("[LORAWAN] Starting OTAA Join Process...");
    int state = node.beginOTAA(joinEui, devEui, appKey, appKey);
    
    if (state < 0) {
        Serial.printf("[ERROR] beginOTAA failed: %d\n", state);
    } else {
        // Start the activation
        state = node.activateOTAA();
        if (state >= 0) {
            Serial.println("[SUCCESS] Node joined network!");
        } else {
            // Common errors: -1116 (No Join Accept), -705 (Busy)
            Serial.printf("[DEBUG] Join state: %d\n", state);
        }
    }

    Wire1.begin(19, 20);
    ina219.begin(&Wire1);
    xTaskCreatePinnedToCore(SamplerTask, "Sampler", 8192, NULL, 1, NULL, 0);
}

void loop() {
    heltec_loop(); 

    currentMa = ina219.getCurrent_mA();

    // Re-join Logic if connection is lost
    if (!node.isActivated()) {
        static unsigned long lastRejoin = 0;
        if (millis() - lastRejoin > 30000) { // Try every 30 seconds
            Serial.println("[LORAWAN] Still not joined. Retrying...");
            node.activateOTAA();
            lastRejoin = millis();
        }
    }

    // Uplink Logic
    static unsigned long lastUplink = 0;
    if (node.isActivated() && phaseComplete && (millis() - lastUplink > 30000)) { // only upload every 30 seconds to avoid getting kicked of the server.
        uint16_t mAP = (uint16_t)(currentMa * 10);
        uint16_t hzP = (uint16_t)(peakFreq * 10);
        uint8_t payload[4] = { (uint8_t)(mAP >> 8), (uint8_t)mAP, (uint8_t)(hzP >> 8), (uint8_t)hzP };
        
        Serial.println("[LORAWAN] Uplinking data...");
        node.sendReceive(payload, 4);
        lastUplink = millis();
        phaseComplete = false; 
    } else if (phaseComplete) {
        phaseComplete = false; 
    }

    // OLED
    static unsigned long lastOLED = 0;
    if (millis() - lastOLED > 500) {
        display.clear();
        display.drawString(0, 0, "Hz: " + String(peakFreq, 1));
        display.drawString(0, 15, "mA: " + String(currentMa, 1));
        String status = node.isActivated() ? "JOINED" : "JOINING...";
        display.drawString(0, 45, status);
        display.display();
        lastOLED = millis();
    }
}
