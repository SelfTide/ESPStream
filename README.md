# ESPStream
WIP-ESPStream

This will just be a place holder for todo's till I get things more laid out.

For information on setup for build enviorment please see refer to:

https://github.com/cnlohr/rawdrawandroid/blob/master/README.md

ESP32:

  Holds code for the ESP32's side of the code. there is a bash script in there to compile the code using arduino-cli. feel free to use your IDE of choice here.
  
  Run arduino-cli_compile.sh from root folder of project. IE. ```#~/ESPStream$ bash arduino-cli_compile.sh <COM PORT>```
  
ESPStream root folder:

  Main project with test case ```ESPStream_test.c```, ESPStreams files are ```ESPStream.c``` and ```ESPStream.h```. there is also a bash script file for debugging that I added to help with this process happly named ```debug_ESPStream_test.sh``` if you feel inclined to do so.
  
  There are a few hardcoded path's in the makefile that should be change to reflect your NDK file path to point to needed include files and libraries.
  
  *need to fix this*
  
  # TODO:
  
  - Resolution control for camera's
  - Image capture input to write current image to storage
  - encryption for image trasmission.
  - multi-platform
# current:
This project in it's current state is a Proof of Concept for using a SPI interface ArduCam to work with ESP32 proto-type boards.

Currently the resolution is set for 320x240 with a whooping 3-ish or so frames per-second.
