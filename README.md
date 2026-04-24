# IoT Individual Assignment
Individual Assignment for IoT course 2026, by Oliver van Douveren

## Layout
### Prelimenaries: Maximum Sampling
### Adaptive Sampling (+Bonus signals)
### Performance (Latency & Power Consumption)
### Internet Connection
### Bonus and Project Setup

## Prelimenaries: Maximum Sampling
While the ESP32 hardware supports sampling rates [up to 2 MHz](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/adc.html), we need to run experiments before we can come up with an actual answer. Luckily, a benchmark exists for this exact purpose: by running two seperate CPU cores in parallel to push and pull data through a queue as fast as possible for 1000 ticks, we can calculate the maximum frequency. 

Running the code supplied in [MaxFrequency.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/MaxFrequency.cpp), yields the following result:

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/MaxFrequency.png)

Note that these specific results are unrealistic, however, since it runs in Wokwi. In any case, we have opted to cap the frequency rate at $100Hz$ which is also feasible for physical hardware and avoids oversampling.

## Adaptive Sampling
Now we will focus on applying the FFT to the example signal specified in the assignment: $2\sin(2\pi * 3 * t) + 4 \sin(2 \pi * 5 * t)$. Using the dual-core architecture of our board, we dedicate one core to generating signal samples and the other to processing said samples. The Sampler collects samples using a `dataQueue` which was given a length of $128$ in this implemntation, since it dictates the accuracy of the FFT. The resolution of our FFT can be calculated with the formula $\Delta f = f_{sampling} / N_{Queue}$. Our initial sample will run at $100Hz$, which gives us a bin width of $0.781Hz$. Afterwards, every 5 seconds the sampled data is used to find a new sampling frequency.

Our implementation is found in [AdaptFrequency.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/AdaptFrequency.cpp).

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/IoT_adapt.png)

We see that the algorithm converges rather quickly to $5Hz$.
By conducting experiments, the queue length was increased to 128 and the minimum sampling rate to $20Hz$, since a lower resolution would lead to an estimate closer to $6Hz$. 
All in all, thanks to our adapative sampling we are able to greatly decrease the CPU load from $100Hz$ to only $20Hz$.

### Bonus signals
Next, I wanted to see what happens when the signal consists of two waves with the same amplitude but different frequencies :
$6\sin(2 \pi * 9 * t) + 6\sin(2 \pi * 7 * t)$. A hypothesis could state that adaptive sampling would overestimate the peak frequency.

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/IoT_bonus1.png)

However, this setup proved to be no problem for our FFT, as it looped between a value of $6.9Hz$ and $7.0Hz$ after two generations.

Finally, I was curious about obtaining a signal with 3 terms such that the lowest frequency has the highest amplitude:
$8\sin(2 \pi * 6 * t) + 3\sin(2 \pi * 10 * t) + 5\sin(2 \pi * 25 * t)$.

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/IoT_bonus2.png)

Also this signal is no problem for our FFT, since it identifies the lowest frequency quickly by converging on $5.9Hz$. Here again, the bin width could explain why the measurement is not perfect.

In all three cases the adaptive sampling method lead to near-perfect results, which would scarcely be improved upon if instead over-sampling were implemented. (The code for this section is also contained within the same AdaptFrequency.cpp but with commented out lines.)

## Performance
### Latency
After implementing a functioning FFT reporter, I turned towards the question of how fast the fourier transform was. The `micros()` function was clearly more suited to capture the resolution at this miniscule timescale instead of using ticks. Using the propper commands, the board's APB was found to be running at 80MHz. Inquiring about the timing ended up paying off, as the FFT was found to take approximately 25ms. This was brought down to 15 ms after switching the ArduinoFFT data type from `double` to `float`. If this project were built on hardware, the time could be reduced further.

### Power measurements
I tried implementing an INA219 component in my Wokwi simulator but that would lead to redundant information. Instead, I opted to simulate the consumption of power for different tasks the board was performing.

Implementation can be found here at [PowerConsumption.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/PowerConsumption.cpp), and provides the following output:

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/IoT_PowerConsumption.png)

The code works by establishing a baseline consumption of 55mA, since the ESP32 board is estimated to draw that much power while the CPU is running ([according to Section 4.1](https://documentation.espressif.com/esp32_datasheet_en.pdf)). Subsequently, a variable load proportional to the sampling frequency (currentFs) is added, representing the energy required to toggle the ADC and move data through memory. To simulate the FFT a conditional increase (adding 35–40mA) is triggered only when the FFT buffer is full. This is illustrated in the last entry for >Power in the output image.

## Internet connection
After struggling to setup MQTT, a functioning version could finally be implemented without crashing. In tandem with the 5 second reports, the system attempts to publish its aggregated findings (specifically the average signal value, the dominant frequency, the current sampling rate and the average execution time). For the implementation, we opted to wrap the findings in a JSON string and to utilize asynchronous communication.

Running [RunMQTT.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/RunMQTT.cpp), yields:

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/IoT_internet.png)

The code is able to establish a connection and receive an IP but publishing the findings results in an error.
That is because the Wokwi simulator requires a subscription that costs funds for WiFi functionality. Physical parts equally so.

## Bonus and Project setup
We did not have time to implement to work on the bonus till the end. The code supplied in [Bonus.cpp](https://github.com/olilucky/IoT_indiv/blob/main/Code/Bonus.cpp) only includes the signal generation, but not the fitlers. The output of this code looks like this:

![output](https://github.com/olilucky/IoT_indiv/blob/main/Images/Iot_Bonus.png)

This image also illustrates the setup I was working with, which is included in the [Project Files](https://github.com/olilucky/IoT_indiv/tree/main/Project%20Files) folder.

