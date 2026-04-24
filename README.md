# IoT Individual Assignment - 
Individual Assignment for IoT course 2026

## Introduction


### Disclaimer
Seeing as I am an Erasmus student, I have practically no experience with implementing a project from the ground up. As a result, a lot of time was invested in learning the basics. For simplicity, I have decided to implement most of the project in a Wokwi simulator.


## Prelimenaries: Maximum Sampling
While the ESP32 hardware supports sampling rates [up to 2 MHz](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/adc.html), we need to run experiments before we can come up with an actual answer. Luckily, a benchmark exists for this exact purpose: by running two seperate CPU cores in parallel to push and pull data through a queue as fast as possible for 1000 ticks, we can calculate the maximum frequency. 

Running the code supplied in [MaxFrequency.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/MaxFrequency.cpp), yields the following result:

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/MaxFrequency.png)

Note that these specific results are unrealistic, however, since it runs in Wokwi. In any case, we have opted to cap the frequency rate at $100Hz$ which is also feasible for physical hardware and avoids oversampling.


## Adaptive Sampling
Now we will focus on applying the FFT to the example signal specified in the assignment: $2\sin(2\pi * 3 * t) + 4 \sin(2 \pi * 5 * t)$. Using the dual-core architecture of our board, we dedicate one core to generating signal samples and the other to processing said samples. The Sampler collects samples using a `dataQueue` which was given a length of $128$ in this implemntation, since it dictates the accuracy of the FFT. The resolution of our FFT can be calculated with the formula $\Delta f = f_{sampling} / N_{Queue}$. Our initial sample will run at $100Hz$, which gives us a bin width of $0.781Hz$.

Our implementation is found in [AdaptFrequency.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/MaxFrequency.cpp).

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/IoT_adapt.png)

We see that the algorithm converges rather quickly to $5Hz$.
By conducting experiments, the queue length was increased to 128 and the minimum sampling rate to $20Hz$, since a lower resolution would lead to an estimate closer to $6Hz$.

### Bonus signals
Next, I wanted to see what happens when the signal consists of two waves with the same amplitude but different frequencies :
$6\sin(2 \pi * 9 * t) + 6\sin(2 \pi * 7 * t)$. 

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/IoT_bonus1.png)

This proved to be no problem for our FFT, as it looped between a value of $6.9Hz$ and $7.0Hz$ after two generations.

Finally, I was curious about obtaining a signal with 3 terms such that the lowest frequency has the highest amplitude:
$8\sin(2 \pi * 6 * t) + 3\sin(2 \pi * 10 * t) + 5\sin(2 \pi * 25 * t)$.

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/IoT_bonus2.png)

Also this signal is no problem for our FFT, since it identifies the lowest frequency quickly by converging on $5.9Hz$. Here again, the bin width could explain why the measurement is not perfect.

### Performance - Timing
After implementing a functioning FFT reporter, I turned towards the question of how fast the fourier transform was. The `micros()` function was clearly more suited to capture the resolution at this miniscule timescale instead of using ticks. Using the propper commands, the board's APB was found to be running at 80MHz. Inquiring about the timing ended up paying off, as the FFT was found to take approximately 25ms. This was brought down to 15 ms after switching the ArduinoFFT data type from `double` to `float`. If this project were built on hardware, the time could be reduced further.

### Performance - Power measurements
I tried implementing an INA219 component in my Wokwi simulator but that would lead to redundant information. Due to this reason and the difficulty to implement it, I have instead o opted not to measure the power.

## Internet connection
After struggling to setup MQTT, the simulator finally managed to get a working WiFi signal. Nevertheless, the plan I was working with 





