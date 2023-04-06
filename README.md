# About ZejfCSeis

ZejfCSeis is a command line program written in C used to collect, store and broadcast data from homemade seismometer. It communicates with microcontroller (Arduino) via USB and can be controlled using simple commands. Once the USB connection is enstablished, ZejfCSeis synchronizes the Arduino sampling rate to exactly match the one selected.

Secondly, ZejfCSeis opens a TCP socket so that remote clients can connect to it and watch incoming data in real time as well as browsing historical data. This is achieved using the [ZejfSeis4](https://github.com/xspanger3770/ZejfSeis4) Java application.

# How to run ZejfCSeis

If you want to try this project yourself, firstly you will need to setup the Arduino and the seismometer itself. Tutorial can be found here: https://github.com/xspanger3770/ZejfCSeis/tree/main/arduino

After that, the easiest way to get ZejfCSeis is to clone this repository locally. 

 ```
git clone git@github.com:xspanger3770/ZejfCSeis.git
 ```
 
 Then inside the `ZejfCSeis` folder, create new folder called `build` and build the project using CMake:
 ```
 cd ZejfCSeis
 mkdir build
 cd build
 cmake ..
 make
 ```
 
 This should create an executable file called zejfcseis_(version). You can move it anywhere you like.
 Run it using:
 ```
 ./zejfcseis_(version) -s <serial port> -i <ip address> -p <port number> -r <sample rate>
 ```
 Where:
 `serial port` is the name of serial port where the Arduino is connected
 `ip address` is the ip adress where a TCP socket will be created.
 `port number` is the TCP port
 `sample rate` is the sample rate in Hz. Recommended value is `40`
 
 The whole command might look like:
 
 ```
 ./zejfcseis_1.4.1 -s /dev/ttyUSB0 -i 192.168.1.100 -p 1234 -r 40
 ```
 
 If everything went well, the program will create a new folder `ZejfCSeis` where the data will be stored and you can now enjoy detecting earthquakes by connecting to the TCP socket with [ZejfSeis4](https://github.com/xspanger3770/ZejfSeis4).
