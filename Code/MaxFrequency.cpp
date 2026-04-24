#include <Arduino.h>

#define TEST_BUFFER_SIZE 2048   
#define BENCHMARK_DURATION 1000 

// Global handles
QueueHandle_t benchmarkQueue;
volatile bool isBenchmarking = false;
volatile uint32_t totalDequeued = 0;

// Shared data packet
typedef struct {
    float val;
    uint32_t timestamp;
} BenchmarkPacket;

// --- PRODUCER (CORE 0) ---
// Simulates a high-speed hardware interrupt or DMA stream
void producer(void *pv) {
    BenchmarkPacket p = { 1.23f, 0 };
    
    while (true) {
        if (isBenchmarking) {
            // Attempt to hammer the queue without blocking
            xQueueSend(benchmarkQueue, &p, 0);
        } else {
            vTaskDelay(pdMS_TO_TICKS(5)); // Low power wait
        }
    }
}

// --- CONSUMER (CORE 1) ---
// Simulates the DSP/FFT processing task
void consumer(void *pv) {
    BenchmarkPacket received;
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    Serial.println(">>> STRESS TEST STARTING...");
    uint32_t start = millis();
    isBenchmarking = true;

    // Tight loop for exactly the duration
    while ((millis() - start) < BENCHMARK_DURATION) {
        if (xQueueReceive(benchmarkQueue, &received, 0) == pdPASS) {
            totalDequeued++;
        }
    }

    isBenchmarking = false;
    uint32_t end = millis();
    
    // Calculate final metrics
    float elapsedSec = (end - start) / 1000.0f;
    float throughputHz = (float)totalDequeued / elapsedSec;

    Serial.println("\n--- BENCHMARK RESULTS ---");
    Serial.printf("Duration:    %lu ms\n", end - start);
    Serial.printf("Total Reads: %u items\n", totalDequeued);
    Serial.printf("Max Read Freq: %.2f Hz\n", throughputHz);
    Serial.println("-------------------------\n");

    vTaskDelete(NULL); 
}

void setup() {
    Serial.begin(115200);
    
    // Create queue for the specific packet type
    benchmarkQueue = xQueueCreate(TEST_BUFFER_SIZE, sizeof(BenchmarkPacket));
    
    if (benchmarkQueue == NULL) {
        Serial.println("CRITICAL: Queue Allocation Failed");
        while(1);
    }

    // Pinning to opposite cores to measure true inter-core IPC performance
    xTaskCreatePinnedToCore(producer, "PROD", 2048, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(consumer, "CONS", 4096, NULL, 5, NULL, 1);
}

void loop() {
}
