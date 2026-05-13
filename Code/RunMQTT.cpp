#include <Arduino.h>
#include <WiFi.h>
#include <heltec_unofficial.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <PubSubClient.h> // Required for MQTT
#include "arduinoFFT.h"

// --- Settings ---
const char* ssid     = "boat";
const char* password = "mymilkshake";
const char* mqtt_server = "broker.hivemq.com"; // Public Broker

// --- Global Data ---
double vReal[128], vImag[128];
volatile double peakFreq = 0.0;
float currentV = 0, currentMa = 0, currentMw = 0;

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_INA219 ina219;
ArduinoFFT<double> FFT; 
TaskHandle_t TaskSample;

// Reconnect to MQTT Broker
void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Create a unique client ID
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            delay(2000);
        }
    }
}

// FFT Sampler Task (Core 0)
void SamplerTask(void * pvParameters) {
    while(true) {
        unsigned long microseconds;
        for (int i = 0; i < 128; i++) {
            microseconds = micros();
            double t = micros() / 1000000.0;
            vReal[i] = 2.0 * sin(2.0 * PI * 3.0 * t) + 4.0 * sin(2.0 * PI * 5.0 * t);
            vImag[i] = 0;
            while (micros() - microseconds < 10000) { yield(); } // 100Hz
        }
        FFT = ArduinoFFT<double>(vReal, vImag, 128, 100.0);
        FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        FFT.compute(FFT_FORWARD);
        FFT.complexToMagnitude();
        peakFreq = FFT.majorPeak();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    heltec_setup();
    heltec_ve(true);
    
    WiFi.begin(ssid, password);
    client.setServer(mqtt_server, 1883);

    Wire1.begin(19, 20);
    ina219.begin(&Wire1);

    xTaskCreatePinnedToCore(SamplerTask, "Sampler", 8192, NULL, 1, &TaskSample, 0);
}

void loop() {
    heltec_loop();

    // 1. Maintain MQTT Connection
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    // 2. Read Sensors
    currentV = ina219.getBusVoltage_V();
    currentMa = ina219.getCurrent_mA();
    currentMw = currentV * currentMa;

    // 3. Publish to MQTT every 2 seconds
    static unsigned long lastMqtt = 0;
    if (millis() - lastMqtt > 2000) {
        // We publish individual topics so they look nice in MQTT Explorer
        client.publish("heltec/energy/voltage", String(currentV).c_str());
        client.publish("heltec/energy/current", String(currentMa).c_str());
        client.publish("heltec/energy/power", String(currentMw).c_str());
        client.publish("heltec/energy/peak_hz", String(peakFreq).c_str());
        lastMqtt = millis();
    }

    // 4. OLED Keep-Alive
    static unsigned long lastOLED = 0;
    if (millis() - lastOLED > 500) {
        heltec_ve(true); // Hammer the power rail
        display.clear();
        display.setFont(ArialMT_Plain_10);
        display.drawString(0, 0, "Signal: " + String(peakFreq, 1) + " Hz");
        display.drawString(0, 12, "Power: " + String(currentMw, 1) + " mW");
        display.drawString(0, 24, "V: " + String(currentV, 2) + "V  mA: " + String(currentMa, 1));

        display.drawString(0, 38, "WiFi: " + (WiFi.status() == WL_CONNECTED ? String("OK") : String("LOST")));
        display.drawString(48, 38, "MQTT: " + (client.connected() ? String("OK") : String("ERR")));
        display.drawString(0, 50, (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("LOST")));
        
        display.display();
        lastOLED = millis();
    }
}
