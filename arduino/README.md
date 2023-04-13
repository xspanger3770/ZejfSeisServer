# Seismometer

The seimometer itself is Lehman type seismometer.

A Lehman type seismometer is a device used to detect ground motion caused by seismic activity. It uses a horizontal beam pendulum that is suspended from a stationary frame. The pendulum has a magnet attached to it and a copper coil is positioned nearby. When ground motion occurs, the magnet moves past the copper coil, inducing an electrical current which can be detected and recorded by a sensitive amplifier or data acquisition system. The Lehman seismometer is designed to be very sensitive, able to detect ground motion on the order of a few micrometers.

## ADC
In my project, an analog to digital converter (ADC) is used to achieve this conversion. The ADC device is wired up to an Arduino microcontroller, which serves as the brain of the system. The Arduino reads the digital data from the ADC and takes care of time synchronization, ensuring that the recorded data is accurately time-stamped. Once the data is prepared by the Arduino, it is sent to ZejfCSeis. This is done via a USB connection, allowing for real-time data transfer and continuous monitoring of seismic activity.

The use of modern digital technology and open-source software tools has greatly improved the accessibility and affordability of seismometers, enabling a wider range of enthusiasts like me to study earthquakes and other seismic events.

Currently, 2 highly accurate ADC's are supported: 24-bit ADS1256 and 32-bit ADS1263 and support for more can be easily added.

### ADS1256

To use the ADS1256 analog to digital converter, you will first need two Arduino libraries: [ADS1256](https://github.com/adienakhmad/ADS1256) and [libFilter](https://github.com/MartinBloedorn/libFilter). Install them as any other Arduino libraries by pasting the folders into Arduino/libraries folder on your computer. After wiring up the ADS1256, use Arduino IDE to upload sketch `ADS1256_Seismo` located in this directory.

Here is the wiring for Arduino Nano - the two wires on the right connect directly to the seimometer main coil:

<img src=https://user-images.githubusercontent.com/100421968/231836782-62f4d2d0-edd1-488e-90dd-6f7269f3b97f.jpg width=600 height=420>

I was using this ADC for about one year and it worked perfectly fine as I was able to detect many earthquakes every day from around the world. Later I switched to the more accurate ADS1263 to push the sensitivity to the limit. 

### ADS1263

Hooking up the ADS1263 is pretty simmilar to the ADS1256. The Arduino library for it can be found here: [ADS126X](https://github.com/Molorius/ADS126X) and after installing it you can upload `ADS1263_Seismo` sketch located in this directory.

__TODO image__
