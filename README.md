# IoT Individual Assignment
Individual Assignment for IoT course 2026, by Oliver van Douveren

## Layout
### Prelimenaries: Maximum Sampling
### Adaptive Sampling (+Bonus signals)
### WiFi Connection
### Cloud Connection
### Project Setup

## Prelimenaries: Maximum Sampling
While the ESP32 hardware supports sampling rates [up to 2 MHz](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/adc.html), we need to run experiments before we can come up with an actual answer. Luckily, a benchmark exists for this exact purpose: by running two seperate CPU cores in parallel to push and pull data through a queue as fast as possible for 1000 ticks, we can calculate the maximum frequency. 

Running the code supplied in [MaxFrequency.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/MaxFrequency.cpp), yields the following result:
![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/max_freq.jpg)

Our result of 38238.00 Hz falls short of the theoretical limit. For our use-case, however, it is more than enough for our present purposes.

### Simulated Maximum Sampling
Running the same code on a Wokwi gives us higher number than the real implementation, though nowhere near the theoretical limit:

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/MaxFrequency.png)

## Adaptive Sampling
Now we will focus on applying the FFT to the example signal specified in the assignment: $2\sin(2\pi * 3 * t) + 4 \sin(2 \pi * 5 * t)$. Using the dual-core architecture of our board, we dedicate one core to generating signal samples and the other to processing said samples. The Sampler collects samples using a `dataQueue` which was given a length of $128$ in this implemntation, since it dictates the accuracy of the FFT. The resolution of our FFT can be calculated with the formula $\Delta f = f_{sampling} / N_{Queue}$. For the performance, we chose to pay attention to the mA, current and energy consumption.

Experimentation has shown that powering the system through wires and the connection of a USB-C for the graphs, did lead to some minor interference in the measurements, though it was only about 1-2%. After implementing a functioning FFT reporter, I turned towards the question of how fast the fourier transform was. The `micros()` function was clearly more suited to capture the resolution at this miniscule timescale instead of using ticks. Using the propper commands, the board's APB was found to be running at 80MHz. Inquiring about the timing ended up paying off, as the FFT was found to take approximately 25ms. This was brought down to 15 ms after switching the ArduinoFFT data type from `double` to `float`. If this project were built on hardware, the time could be reduced further.

### 100Hz Sampling
Our initial sample will run at $100Hz$, which gives us a bin width of $0.781Hz$. Afterwards, every 5 seconds the sampled data is used to find a new sampling frequency. In our implementation we added a resting phase and a measuring phase, in order to distinguish between the actual FFT and the background processes.

The code of our implementation is found in [MaxFrequency.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/MaxFrequency.cpp).

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/measurements_100.png)

As we see in our graph, there is not a lot of difference in terms of power consumption between the two phases. Nevertheless, there is a slight increase when measuring which is to be expected.

### Adaptive

Next, for the adaptive sampling we collect data for 5 seconds, then perform the FFT and then adjust the sampling frequency accordingly. 

Our implementation is found in [AdaptFrequency.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/AdaptFrequency.cpp). The yields the following readings:

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/measurements_adaptive.png)

Although the peak frequency is only 5Hz, we set the minimum sampling rate to 20Hz such that this is attained throughout the entire experiment. All in all, thanks to our adapative sampling we are able to greatly decrease the CPU load from $100Hz$ to only $20Hz$. Nonetheless, the energy efficiency gained is smaller than I had expected, though there is some improvement. This may be due to the prevalence of background processes.

### Bonus signals (Simulated)
For the bonus, I was curious to see what happens when the signal consists of two waves with the same amplitude but different frequencies :
$6\sin(2 \pi * 9 * t) + 6\sin(2 \pi * 7 * t)$. A hypothesis could state that adaptive sampling would overestimate the peak frequency.

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/IoT_bonus1.png)

However, this setup proved to be no problem for our FFT, as it looped between a value of $6.9Hz$ and $7.0Hz$ after two generations.

Finally, I was curious about obtaining a signal with 3 terms such that the lowest frequency has the highest amplitude:
$8\sin(2 \pi * 6 * t) + 3\sin(2 \pi * 10 * t) + 5\sin(2 \pi * 25 * t)$.

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/IoT_bonus2.png)

Also this signal is no problem for our FFT, since it identifies the lowest frequency quickly by converging on $5.9Hz$. Here again, the bin width could explain why the measurement is not perfect.

In all three cases the adaptive sampling method lead to near-perfect results, which would scarcely be improved upon if instead over-sampling were implemented. (The code for this section is also contained within the same AdaptFrequency.cpp but with commented out lines.)

## WiFi and MQTT
After struggling to setup MQTT, we settled on having 

Running [RunMQTT.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/RunMQTT.cpp), yields:

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/measurements_wifi.png)

We see that approximately every 15 seconds a spike occurs in which the data is transfered to the server. 

## Cloud connection
For this section, I opted to use The Things Network. Also this was hard to setup, though I eventually managed to communicate data with the cloud as seen here:

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/cloud_computing.png)

In order to avoid getting kicked of the server, I added a timer of 30000 miliseconds before sending each packet, as can be seen in the implemnation here [LoRa.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/LoRa.cpp). The readings of the board did not show measurements that deviated much from what has been seen before. 


## Project setup
We did not have time to implement to work on the bonus till the end. The code supplied in [Bonus.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/Bonus.cpp) only includes the signal generation, but not the fitlers. The output of this code looks like this:

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/Iot_Bonus.png)

This image also illustrates the setup I was working with, which is included in the [Project Files](https://github.com/olilucky/IoT_indiv/tree/main/Project%20Files) folder.

