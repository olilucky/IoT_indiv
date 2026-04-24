#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <arduinoFFT.h>

// --- Global Objects ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- FFT & Sampling Variables ---
#define SAMPLES 128 
float vReal[SAMPLES];
float vImag[SAMPLES];

// Instantiate FFT object
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, 100.0);

volatile float currentFs = 100.0;
volatile float lastPeakFreq = 0.0;
QueueHandle_t dataQueue;

// --- Signal Parameters (Bonus Question) ---
#define SIGMA_NOISE 0.2    // n(t)
#define ANOMALY_PROB 0.02  // p = 0.02
#define ANOMALY_MIN 5.0    // U(5, 15)
#define ANOMALY_MAX 15.0

// --- Function Prototypes ---
void SamplerTask(void *pvParameters);
void ProcessingTask(void *pvParameters);

void setup() {
    Serial.begin(115200);

    // Hardware Clock Info
    Serial.println("\n--- IoT Robust Monitor Initialized ---");
    Serial.printf("CPU Frequency: %u MHz\n", getCpuFrequencyMhz());
    
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();
    lcd.print("Anomaly Monitor");

    dataQueue = xQueueCreate(128, sizeof(float)); 
    if (dataQueue != NULL) {
        xTaskCreatePinnedToCore(SamplerTask, "Sampler", 4096, NULL, 2, NULL, 1);
        xTaskCreatePinnedToCore(ProcessingTask, "Processor", 8192, NULL, 1, NULL, 0);
    }
}

void loop() {}

// --- Sampler Task (Core 1) ---
// Implements: s(t) = 2*sin(2pi*3t) + 4*sin(2pi*5t) + n(t) + A(t)
void SamplerTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    float t = 0;
    
    while (1) {
        // 1. Clean Signal
        float clean = 2.0 * sin(2.0 * PI * 3.0 * t) + 4.0 * sin(2.0 * PI * 5.0 * t);
        
        // 2. n(t): Gaussian Noise (Approx via sum of randoms)
        float nt = ((float)random(-100, 101) / 100.0) * SIGMA_NOISE;
        
        // 3. A(t): Anomaly Injection
        float At = 0;
        if ((float)random(0, 1000) < (ANOMALY_PROB * 1000)) {
            // Random spike between +/- (5 to 15)
            float magnitude = ANOMALY_MIN + (float)random(0, (int)(ANOMALY_MAX - ANOMALY_MIN));
            At = (random(0, 2) == 0) ? magnitude : -magnitude;
            
            // Log anomaly occurrence for debugging
            Serial.print("!ANOMALY_INJECTED:"); Serial.println(At);
        }

        float s_t = clean + nt + At;
        
        xQueueSend(dataQueue, &s_t, 0);
        
        // Teleplot live stream
        Serial.print(">Signal:"); Serial.println(s_t);
        
        t += (1.0 / currentFs);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000.0 / currentFs));
    }
}

// --- Processing Task (Core 0) ---
void ProcessingTask(void *pvParameters) {
    float sample;
    int fftCount = 0;
    unsigned long windowStartTime = millis();

    while (1) {
        if (xQueueReceive(dataQueue, &sample, portMAX_DELAY)) {
            vReal[fftCount] = sample;
            vImag[fftCount] = 0.0f; // Reset imaginary part to prevent NaN
            fftCount++;

            if (fftCount == SAMPLES) {
                uint32_t startU = micros(); 

                // FFT Calculation
                FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
                FFT.compute(FFT_FORWARD);
                FFT.complexToMagnitude();
                
                // Find Peak
                float peakFrequency = FFT.majorPeak();
                lastPeakFreq = peakFrequency;
                
                // Adaptive sampling logic 
                // We target 4x peak to ensure Nyquist even with noise
                float newFs = max(lastPeakFreq * 4.0, 25.0); 
                currentFs = constrain(newFs, 25.0, 180.0);

                uint32_t executionTime = micros() - startU;

                // Teleplot Metrics
                Serial.print(">PeakFreq:"); Serial.println(lastPeakFreq);
                Serial.print(">SamplingFs:"); Serial.println(currentFs);
                Serial.print(">FFT_Time_us:"); Serial.println(executionTime);

                fftCount = 0;
            }

            // LCD Update every 2 seconds
            if (millis() - windowStartTime >= 2000) {
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.printf("Peak: %.1f Hz", lastPeakFreq);
                lcd.setCursor(0, 1);
                lcd.printf("Fs: %.0f Hz", currentFs);
                windowStartTime = millis();
            }
        }
    }
}
