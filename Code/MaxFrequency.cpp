#include <Arduino.h>
#include <heltec_unofficial.h>

// --- Configuration ---
#define TEST_DURATION_MS 5000   
#define QUEUE_SIZE 512          

typedef struct {
    float val;
    uint32_t timestamp;
} SensorPacket;

QueueHandle_t dataQueue;
volatile bool isBenchmarking = false;
volatile bool testFinished = false;
volatile uint32_t samplesProduced = 0;
float finalResultHz = 0;

// --- PRODUCER (CORE 0): The High-Speed Sampler ---
void producerTask(void *pv) {
    SensorPacket p;
    while (true) {
        if (isBenchmarking) {
            float t = micros() / 1000000.0f;
            // The assignment's required input signal
            p.val = 2.0f * sin(2.0f * PI * 3.0f * t) + 4.0f * sin(2.0f * PI * 5.0f * t);
            p.timestamp = micros();

            if (xQueueSend(dataQueue, &p, 0) == pdPASS) {
                samplesProduced++;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10)); 
        }
    }
}

// --- CONSUMER (CORE 1): The Controller ---
void consumerTask(void *pv) {
    SensorPacket received;
    
    // 1. BOOTING PHASE
    vTaskDelay(pdMS_TO_TICKS(2000)); 
    
    // 2. RUNNING PHASE
    isBenchmarking = true;
    uint32_t startTime = millis();

    while ((millis() - startTime) < TEST_DURATION_MS) {
        // Drain the queue as fast as possible
        xQueueReceive(dataQueue, &received, 0);
    }

    // 3. CONCLUDED PHASE
    isBenchmarking = false;
    uint32_t endTime = millis();
    float elapsedSec = (endTime - startTime) / 1000.0f;
    finalResultHz = (float)samplesProduced / elapsedSec;
    testFinished = true;

    Serial.printf("\n[BENCHMARK] Result: %.2f Hz\n", finalResultHz);

    vTaskDelete(NULL); 
}

void setup() {
    heltec_setup(); // Hardware and OLED Init
    display.setContrast(255);
    
    Serial.begin(115200);

    dataQueue = xQueueCreate(QUEUE_SIZE, sizeof(SensorPacket));
    
    if (dataQueue == NULL) {
        display.clear();
        display.drawString(0, 0, "Queue Error!");
        display.display();
        while(1);
    }

    xTaskCreatePinnedToCore(producerTask, "PROD", 4096, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(consumerTask, "CONS", 4096, NULL, 10, NULL, 1);
}

void loop() {
    // We handle the OLED in the main loop to keep it responsive
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 100) {
        display.clear();
        display.setFont(ArialMT_Plain_10);

        if (!isBenchmarking && !testFinished) {
            display.drawString(0, 0, "STATUS: BOOTING...");
            display.drawString(0, 20, "Initializing tasks...");
        } 
        else if (isBenchmarking) {
            display.drawString(0, 0, "STATUS: RUNNING");
            display.drawString(0, 20, "Sampling Sine Sum...");
            display.drawString(0, 40, "Samples: " + String(samplesProduced));
            // Simple progress bar
            display.drawProgressBar(0, 55, 128, 8, (samplesProduced % 100)); 
        } 
        else if (testFinished) {
            display.setFont(ArialMT_Plain_16);
            display.drawString(0, 0, "FINISHED");
            display.setFont(ArialMT_Plain_10);
            display.drawString(0, 25, "Max Sampling Freq:");
            display.setFont(ArialMT_Plain_16);
            display.drawString(0, 40, String(finalResultHz, 2) + " Hz");
        }

        display.display();
        lastUpdate = millis();
    }
}
