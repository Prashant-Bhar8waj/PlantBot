#!/usr/bin/env pybricks-micropython
from pybricks.ev3devices import Motor
from pybricks.iodevices import UARTDevice
from pybricks.parameters import Port
from pybricks.robotics import DriveBase

left = Motor(Port.B)
right = Motor(Port.C)
drive = DriveBase(left, right, 30, 175)

drive.settings(straight_speed=150, turn_rate=75)

ser = UARTDevice(Port.S1, baudrate=9600, timeout=None) # pyright: ignore[reportArgumentType]

while True:
    data = ser.read()
    if data == b's':
        arg_length = ser.read()
        distance = int(ser.read(int.from_bytes(arg_length, 'big')).decode())
        drive.straight(distance)
        ser.write(b"!")
    elif data == b't':
        arg_length = ser.read()
        angle = int(ser.read(int.from_bytes(arg_length, 'big')).decode())
        drive.turn(angle)
        ser.write(b"!")
    elif data == b'd':
        arg_length = ser.read()
        args = ser.read(int.from_bytes(arg_length, 'big')).decode()
        speed = int(args.split(';')[0])
        turn_rate = int(args.split(';')[1])
        drive.drive(speed, turn_rate)
        ser.write(b"!")
    elif data == b'r':
        value_str = str(drive.distance())
        ser.write(len(value_str).to_bytes(1, 'big'))
        ser.write(bytes(value_str, 'utf-8'))
    elif data == b'h':
        drive.stop()
        ser.write(b"!")
