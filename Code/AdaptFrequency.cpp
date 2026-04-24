#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <arduinoFFT.h>

// --- Global Objects ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- FFT & Sampling Variables ---
#define SAMPLES 64
float vReal[SAMPLES];
float vImag[SAMPLES];

// Instantiate FFT object with float type
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, 100.0);

volatile float currentFs = 200.0;
volatile float lastPeakFreq = 0.0;
QueueHandle_t dataQueue;

// --- Function Prototypes ---
void SamplerTask(void *pvParameters);
void ProcessingTask(void *pvParameters);

void setup() {
    Serial.begin(115200);

    // Hardware Clock Info
    uint32_t cpuFreq = getCpuFrequencyMhz();
    uint32_t apbFreq = getApbFrequency();
    Serial.println("\n--- Hardware Clock Info ---");
    Serial.printf("CPU Frequency: %u MHz\n", cpuFreq);
    Serial.printf("APB Frequency: %u Hz\n", apbFreq);
    Serial.println("---------------------------");
    
    // 1. I2C & LCD Initialization
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();
    lcd.print("System Ready");

    // 2. FreeRTOS Tasks
    dataQueue = xQueueCreate(64, sizeof(float));  //Queue length
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
        //float val = 6.0 * sin(2.0 * PI * 9.0 * t) + 6.0 * sin(2.0 * PI * 7.0 * t);
        //float val = 6.0 * sin(2.0 * PI * 9.0 * t) + 6.0 * sin(2.0 * PI * 7.0 * t) + 6.0 * sin(2.0 * PI * 9.0 * t);
        
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
    unsigned long queueFullTime = 0;

    // Metrics Tracking
    uint32_t lastExecutionTime = 0;
    uint32_t totalExecTime = 0; 
    int fftCalculationsInWindow = 0;

    while (1) {
        if (xQueueReceive(dataQueue, &sample, portMAX_DELAY)) {
            
            // 1. Accumulate statistics
            runningSum += sample;
            samplesProcessed++;
            
            // 2. Fill FFT Buffer
            vReal[fftCount] = sample;
            vImag[fftCount] = 0.0f;
            fftCount++;

            // 3. Perform FFT
            if (fftCount == SAMPLES) {
                uint32_t startU = micros(); 

                FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
                FFT.compute(FFT_FORWARD);
                FFT.complexToMagnitude();
                lastPeakFreq = (float)FFT.majorPeak();
                
                // Adaptive sampling logic
                float newFs = lastPeakFreq * 2.5;
                currentFs = constrain(newFs, 15.0, 200.0);

                uint32_t endU = micros(); 
                
                lastExecutionTime = endU - startU;
                totalExecTime += lastExecutionTime;
                fftCalculationsInWindow++;
                queueFullTime = millis();

                fftCount = 0;
            }

            // 4. 5-Second Aggregation Report
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
