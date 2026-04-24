#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <arduinoFFT.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- Configuration Constants ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "3.125.131.139";
const char* topic_telemetry = "esp32/sensor/data";

// --- Global Objects ---
WiFiClient espClient;
PubSubClient mqttClient(espClient);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- FFT & Sampling Variables ---
#define SAMPLES 64
float vReal[SAMPLES];
float vImag[SAMPLES];

// Instantiate FFT object with float type
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, 100.0);

volatile float currentFs = 100.0;
volatile float lastPeakFreq = 0.0;
QueueHandle_t dataQueue;
unsigned long totalMessagesSent = 0;

// --- Function Prototypes ---
void SamplerTask(void *pvParameters);
void ProcessingTask(void *pvParameters);
void reconnectMQTT();

// --- MQTT Reconnection Logic ---
void reconnectMQTT() {
    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32Client-" + String(random(0xffff), HEX);
        
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("Connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" (Will retry in 10s)");
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Hardware Clock Info
    uint32_t cpuFreq = getCpuFrequencyMhz();
    uint32_t apbFreq = getApbFrequency();
    Serial.println("\n--- Hardware Clock Info ---");
    Serial.printf("CPU Frequency: %u MHz\n", cpuFreq);
    Serial.printf("APB Frequency: %u Hz\n", apbFreq);
    Serial.println("---------------------------");
    
    // 1. WiFi Connection (Standard Wokwi Guest Setup)
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" Connected! IP: ");
    Serial.println(WiFi.localIP());

    // 2. Setup MQTT
    mqttClient.setServer(mqtt_server, 1883);
    
    // 3. I2C & LCD Initialization
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();
    lcd.print("IoT Initialized");

    // 4. FreeRTOS Tasks
    dataQueue = xQueueCreate(64, sizeof(float)); 
    if (dataQueue != NULL) {
        xTaskCreatePinnedToCore(SamplerTask, "Sampler", 4096, NULL, 2, NULL, 1);
        xTaskCreatePinnedToCore(ProcessingTask, "Processor", 8192, NULL, 1, NULL, 0);
        Serial.println("System Tasks Started");
    }
}

void loop() {
    // Main loop stays empty in FreeRTOS
}

// --- Sampler Task (Core 1) ---
void SamplerTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    float t = 0;
    while (1) {
        // Simulated Signal
        float val = 2.0 * sin(2.0 * PI * 3.0 * t) + 4.0 * sin(2.0 * PI * 5.0 * t);
        
        xQueueSend(dataQueue, &val, 0);
        
        t += (1.0 / currentFs);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000.0 / currentFs));
    }
}

// --- Processing Task (Core 0) ---
void ProcessingTask(void *pvParameters) {
    float sample;
    int fftCount = 0;
    float runningSum = 0;
    int samplesProcessed = 0;
    unsigned long windowStartTime = millis();
    unsigned long lastMqttRetry = 0;
    unsigned long queueFullTime = 0;

    // Metrics Tracking
    uint32_t lastExecutionTime = 0;
    uint32_t totalExecTime = 0; 
    int fftCalculationsInWindow = 0;

    while (1) {
        if (xQueueReceive(dataQueue, &sample, portMAX_DELAY)) {
            
            // 1. MQTT Connection Management
            if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
                unsigned long now = millis();
                if (now - lastMqttRetry > 10000) {
                    reconnectMQTT();
                    lastMqttRetry = now;
                }
            }
            mqttClient.loop();

            // 2. Accumulate statistics
            runningSum += sample;
            samplesProcessed++;
            
            // 3. Fill FFT Buffer
            vReal[fftCount] = sample;
            vImag[fftCount] = 0.0f;
            fftCount++;

            // 4. Perform FFT
            if (fftCount == SAMPLES) {
                uint32_t startU = micros(); 

                FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
                FFT.compute(FFT_FORWARD);
                FFT.complexToMagnitude();
                lastPeakFreq = (float)FFT.majorPeak();
                
                // Adaptive sampling logic
                float newFs = lastPeakFreq * 2.5;
                currentFs = constrain(newFs, 15.0, 500.0);

                uint32_t endU = micros(); 
                
                lastExecutionTime = endU - startU;
                totalExecTime += lastExecutionTime;
                fftCalculationsInWindow++;
                queueFullTime = millis();

                fftCount = 0;
            }

            // 5. 5-Second Aggregation Report
            if (millis() - windowStartTime >= 5000) {
                unsigned long reportLatency = millis() - queueFullTime;
                float avg = (samplesProcessed > 0) ? (runningSum / samplesProcessed) : 0;
                uint32_t avgExec = (fftCalculationsInWindow > 0) ? (totalExecTime / fftCalculationsInWindow) : 0;

                // Serial Output
                Serial.printf("\n--- 5s Aggregate Report ---\n");
                Serial.printf("Execution: %u us (Avg: %u us)\n", lastExecutionTime, avgExec);
                Serial.printf("Latency:   %lu ms\n", reportLatency);
                Serial.printf("Signal:    Avg %.3f V | Peak %.1f Hz\n", avg, lastPeakFreq);
                Serial.printf("Sampling:  Fs %.1f Hz\n", currentFs);
                Serial.println("---------------------------");

                // MQTT Publish
                if (mqttClient.connected()) {
                    char msg[128];
                    snprintf(msg, 128, "{\"avg_v\":%.2f, \"peak_hz\":%.1f, \"fs\":%.1f, \"exec_us\":%u}", 
                             avg, lastPeakFreq, currentFs, avgExec);
                    
                    if (mqttClient.publish(topic_telemetry, msg)) {
                        totalMessagesSent++;
                        Serial.println("MQTT Publish: Success");
                    }
                }

                // LCD Update
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.printf("Avg:%.2f Pk:%.1f", avg, lastPeakFreq);
                lcd.setCursor(0, 1);
                lcd.printf("Fs:%.0f Lat:%ums", currentFs, (uint32_t)reportLatency);

                // Reset Window
                runningSum = 0;
                samplesProcessed = 0;
                totalExecTime = 0;
                fftCalculationsInWindow = 0;
                windowStartTime = millis();
            }
        }
    }
}
