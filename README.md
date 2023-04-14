# About ZejfSeisServer

ZejfSeisServer is a command line program written in C used to collect, store and broadcast data from homemade seismometer. It communicates with microcontroller (Arduino) via USB and can be controlled using simple commands. Once the USB connection is enstablished, ZejfSeisServer synchronizes the Arduino sampling rate to exactly match the one selected.

ZejfSeisServer opens a TCP socket so that remote clients can connect to it and watch incoming data in real time as well as browsing historical data. This is achieved using the [ZejfSeis](https://github.com/xspanger3770/ZejfSeis) Java application. Here is what you can expect after setting up the seismometer: the image shows a magnitude 3.3 earthquake at a 170km distance.

![best_screenshot](https://user-images.githubusercontent.com/100421968/231841190-64a4e8ea-822e-417d-ae36-a20a293d10fd.png)


# How to run ZejfSeisServer

**__Tutorial is work in progress!__**

The easiest way to get ZejfSeisServer is to clone this repository locally. 

 ```
git clone git@github.com:xspanger3770/ZejfSeisServer.git
 ```
 
 Then inside the `ZejfSeisServer` folder, create new folder called `build` and build the project using CMake:
 ```
 cd ZejfSeisServer
 mkdir build
 cd build
 cmake ..
 make
 ```
 
 This should create an executable file called zejfseis_server_(version). You can move it anywhere you like.
 Run it using:
 ```
 ./zejfseis_server_(version) -s <serial port> -i <ip address> -p <port number> -r <sample rate>
 ```
 Where:
 `serial port` is the name of serial port where the Arduino is connected
 `ip address` is the ip adress where a TCP socket will be created.
 `port number` is the TCP port
 `sample rate` is the sample rate in Hz. Recommended value is `40`
 
 The whole command might look like:
 
 ```
 ./zejfseis_server_1.5.0 -s /dev/ttyUSB0 -i 192.168.1.100 -p 1234 -r 40
 ```
 
 If everything went well, the program will create a new folder `ZejfSeis_Server` where the data will be stored and you can now enjoy detecting earthquakes by connecting to the TCP socket with [ZejfSeis](https://github.com/xspanger3770/ZejfSeis).
