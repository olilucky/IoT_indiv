#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <arduinoFFT.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define SAMPLES 64
float vReal[SAMPLES];
float vImag[SAMPLES];
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, 100.0);

volatile float currentFs = 100.0;
volatile float lastPeakFreq = 0.0;
QueueHandle_t dataQueue;
float currentPower_mA = 50.0; 

void SamplerTask(void *pvParameters);
void ProcessingTask(void *pvParameters);

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();
    lcd.print("Graphing Mode");

    dataQueue = xQueueCreate(SAMPLES, sizeof(float)); 
    if (dataQueue != NULL) {
        xTaskCreatePinnedToCore(SamplerTask, "Sampler", 4096, NULL, 2, NULL, 1);
        xTaskCreatePinnedToCore(ProcessingTask, "Processor", 8192, NULL, 1, NULL, 0);
    }
}

void loop() { vTaskDelete(NULL); }

void SamplerTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    float t = 0;
    while (1) {
        float val = 2.0 * sin(2.0 * PI * 3.0 * t) + 4.0 * sin(2.0 * PI * 5.0 * t);
        xQueueSend(dataQueue, &val, 0);
        t += (1.0 / currentFs);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000.0 / currentFs));
    }
}

void ProcessingTask(void *pvParameters) {
    float sample;
    int fftCount = 0;
    while (1) {
        if (xQueueReceive(dataQueue, &sample, portMAX_DELAY)) {
            currentPower_mA = 55.0 + (currentFs / 15.0);
            vReal[fftCount] = sample;
            vImag[fftCount] = 0.0f;
            fftCount++;

            if (fftCount == SAMPLES) {
                currentPower_mA += 35.0; // CPU spike
                FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
                FFT.compute(FFT_FORWARD);
                FFT.complexToMagnitude();
                lastPeakFreq = (float)FFT.majorPeak();
                currentFs = constrain(lastPeakFreq * 2.5, 20.0, 150.0);
                fftCount = 0;
            }

            // --- THE BETTER WAY: SERIAL TELEMETRY ---
            // No libraries, no WiFi. Just print the Teleplot "Magic Prefix"
            Serial.print(">Power:");   Serial.println(currentPower_mA);
            Serial.print(">Signal:");  Serial.println(sample);
            Serial.print(">PeakFreq:"); Serial.println(lastPeakFreq);
        }
    }
}
