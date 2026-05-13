#include <Arduino.h>
#include <heltec_unofficial.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "arduinoFFT.h"

// --- Configuration ---
#define SAMPLES 128           
#define AGGREGATION_WINDOW_MS 5000 

double vReal[SAMPLES], vImag[SAMPLES];
float currentMa = 0, busVoltage = 0, powerMw = 0;
float samplingFreq = 100.0; 
float detectedPeak = 0;

uint32_t lastProcessingTimeUs = 0;
uint32_t totalFFTsInWindow = 0;

Adafruit_INA219 ina219;
ArduinoFFT<double> FFT;

void setup() {
    heltec_setup();
    Serial.begin(115200);
    
    Wire1.begin(19, 20);
    if (!ina219.begin(&Wire1)) {
        Serial.println("[ERROR] INA219 not found");
    }
    
    display.setContrast(255);
}

void loop() {
    uint32_t windowStart = millis();
    totalFFTsInWindow = 0;

    // --- 5 SECOND AGGREGATION WINDOW ---
    while (millis() - windowStart < AGGREGATION_WINDOW_MS) {
        
        // --- 1. SENSING PHASE ---
        float sampling_period_us = 1000000.0 / samplingFreq;
        for (int i = 0; i < SAMPLES; i++) {
            uint32_t startSample = micros();
            
            // Input Signal Simulation
            float t = startSample / 1000000.0f;
            vReal[i] = 2.0 * sin(2.0 * PI * 3.0 * t) + 4.0 * sin(2.0 * PI * 5.0 * t);
            vImag[i] = 0;

            // Continuous Power Metrics for Teleplot
            currentMa = ina219.getCurrent_mA();
            busVoltage = ina219.getBusVoltage_V();
            powerMw = busVoltage * currentMa;
            
            // Teleplot Stream: V-mA-W-Hz
            Serial.printf(">Voltage_V:%.2f\n", busVoltage);
            Serial.printf(">Current_mA:%.2f\n", currentMa);
            Serial.printf(">Power_mW:%.2f\n", powerMw);
            Serial.printf(">Sampling_Hz:%.2f\n", samplingFreq);

            // Maintain Sampling Rate
            while (micros() - startSample < sampling_period_us) { yield(); }
        }

        // --- 2. ANALYSIS PHASE (Processing Latency) ---
        uint32_t anaStart = micros();
        
        FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, samplingFreq);
        FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        FFT.compute(FFT_FORWARD);
        FFT.complexToMagnitude();
        detectedPeak = (float)FFT.majorPeak();

        // Adaptive Frequency Adjustment
        if (detectedPeak > 0) {
            samplingFreq = constrain(detectedPeak * 4.0, 20.0, 200.0);
        }
        
        lastProcessingTimeUs = micros() - anaStart; // Actual calculation time
        totalFFTsInWindow++;
    }

    // --- REPORTING PHASE (OLED) ---
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "Freq: " + String(samplingFreq, 1) + " Hz");
    display.drawString(0, 15, "Latency: " + String(lastProcessingTimeUs / 1000.0, 2) + " ms");
    display.drawString(0, 30, "Current: " + String(currentMa, 1) + " mA");
    display.drawString(0, 45, "FFT Cycles: " + String(totalFFTsInWindow));
    display.display();

    // Serial Summary
    Serial.printf("\n[WINDOW CLOSED] Latency: %.2f ms | Avg Freq: %.2f Hz\n\n", 
                  lastProcessingTimeUs / 1000.0, samplingFreq);
}
