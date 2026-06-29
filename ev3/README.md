## (For Now) Standalone EV3/ESP base

This folder contains two important files:
- `move.py` is the script to be executed on the EV3
- `esp_driver.cpp` is the ESP32/8266-side driver code for the host

## EV3 client

The python script running on the EV3 contains a physical model (provided by the pybricks firmware) to simulate the drive base and allow use to move by specified physical units (millimeters, in this case). When running, the program opens a UART connection on sensor port 1 at 9600 baud. It then listens for commands, encoded as a single byte command identifier followed by a length-prefixed, semicolon-separated argument string. The following commands are supported:
- **`s`: Move in a straight line**  
  Takes one integer argument and moves the specified distance in millimeters  
  *Responds when the robot is done moving*
- **`t`: Turn in place**  
  Takes one integer argument and turns the specified angle in degrees in place  
  *Responds when the robot is done moving*
- **`d`: Drive**  
  Takes two integer arguments (`speed` and `turn_rate`) and then starts moving. The robot covers `speed` millimeters of distance every second while turning at `turn_rate` degrees per second  
  *Responds immediately*
- **`r`: Read distance**  
  Takes no arguments and responds with the distance in millimeters the robot has moved from the origin, encoded as a length-prefixed string  
  *Responds immediately*
- **`h`: Halt**  
  Takes no arguments and stops the robot if it is currently moving  
  *Responds immediately*

## ESP Host

The ESP host program exposes a number of driver functions (reading the position, moving to an absolute offset, etc) which implement the protocol specified above. In its current configuration, the repositioning routine is triggered externally by a button attached to the ESP. It then measures the light intensity at a predefined number of spots along the predefined moving range. Immediately after, it moves the robot to the exact offset where the brightest light was measured.
