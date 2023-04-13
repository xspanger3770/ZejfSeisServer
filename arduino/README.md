# Seismometer

The seismometer uses an analog to digital converter (ADC) device to convert the voltage created by the seismometer coil to digital data. The ADC is wired up to Arduino microcontroller that reads the data and takes care of time synchronization. Arduino then sends the prepared data to ZejfCSeis via USB connection.

Currently, 2 highly accurate ADC's are supported: 24-bit ADS1256 and 32-bit ADS1263 and more can be easily added.

## ADS1256

To use the ADS1256 analog to digital converter, you will first need two Arduino libraries: [ADS1256](https://github.com/adienakhmad/ADS1256) and [libFilter](https://github.com/MartinBloedorn/libFilter). Install them as any other Arduino libraries by pasting the folders into Arduino/libraries folder on your computer. After wiring up the ADS1256, use Arduino IDE to upload sketch `ADS1256_Seismo` located in this directory.

Here is the wiring for Arduino Nano - the two wires on the right connect directly to the seimometer main coil:

<img src=https://user-images.githubusercontent.com/100421968/231836782-62f4d2d0-edd1-488e-90dd-6f7269f3b97f.jpg width=600 height=420>

I was usinng this ADC for about one year and it worked perfectly fine as I was able to detect many earthquakes every day from around the world. Later I switched to the more accurate ADS1263 to push the sensitivity to the limit. 

## ADS1263

Hooking up the ADS1263 is pretty simmilar to the ADS1256. The Arduino library for it can be found here: [ADS126X](https://github.com/Molorius/ADS126X) and after installing it you can upload `ADS1263_Seismo` sketch located in this directory.
