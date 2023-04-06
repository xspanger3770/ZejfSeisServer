# About ZejfCSeis

ZejfCSeis is a command line program written in C used to collect, store and broadcast data from homemade seismometer. It communicates with microcontroller (Arduino) via USB and can be controlled using simple commands. Once the USB connection is enstablished, ZejfCSeis synchronizes the Arduino sampling rate to exactly match the one selected.

Secondly, ZejfCSeis opens a TCP socket so that remote clients can connect to it and watch incoming data in real time as well as browsing historical data. This is achieved using the [ZejfSeis4](https://github.com/xspanger3770/ZejfSeis4) Java application.
