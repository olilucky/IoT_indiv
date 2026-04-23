# IoT Individual Assignment - 
Individual Assignment for IoT course 2026

## Introduction


### Disclaimer
Seeing as I am an Erasmus student, I have practically no experience with implementing a project from the ground up. As a result, a lot of time was invested in learning the basics. 
For simplicity, I have decided to implement most of the project in a Wokwi simulator.

## Adapative Sampling
### Prelimenaries: Maximum Sampling
Before getting into the implementation, we need to reason about the maximum sampling rate of our device

### Adaptive Sampling
Now we will focus on applying the FFT to the example signal specified in the assignment: $2\sin(2\pi*3*t)+4\sin(2\pi*5*t)$

### Bonus
Signals considered:

Next, I wanted to see what happens when two signals with the same amplitude but different frequencies get mixed. I therefore decided to consider
$6\sin(2 \pi * 9 * t) + 6\sin(2 \pi * 7 * t)$

Finally, I was curious about obtaining a signal with 3 terms such that the lowest frequency has the highest amplitude
$8\sin(2 \pi * 6 * t) + 3\sin(2 \pi * 10 * t) + 5\sin(2 \pi * 25 * t)$

### Physical Implementation


## Internet connection
After struggling with MQTT

## System Performance
### Timing
After implementing a functioning FFT reporter, I turned towards the question of how fast the fourier transform was. The 'micros()' function was clearly more suited to capture the resolution at this miniscule timescale instead of using ticks. Using the propper commands I was able to find that my board's APB was running at 80MHz

### Power measurements
I tried implementing an INA219 component in my Wokwi simulator but that would lead to redundant information. Due to this reason and the difficulty to implement it, I have opted not to measure the power.
