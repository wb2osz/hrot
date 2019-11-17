#!/usr/bin/env python3

# "Ham Radio of Things" example.
# WB2OSZ, November 2019

# Use this as a starting point for your own application.

# Subscribe to the "rgbled" topic and set tri-color LED from messages.
# Subscribe to the "servo" topic and set servo position from messages.

import hrot
import RPi.GPIO as GPIO

chan = 0		# Radio channel for multi-port TNC.

mycall = 'WB2OSZ-1'	# Name of my "thing" node.  
			# >>>>>> CHANGE THIS to use your own call. <<<<<<

broker = 'WB2OSZ-15'	# Station ID for the Broker.  
			#Could be a shared community resource.
via = 'WIDE1-1'

# List of topics for your actuators.

rgbled_topic = 'rgbled'
servo_topic = 'servo'
# etc.

hrot.subscribe (chan, mycall, via, broker, rgbled_topic)
hrot.subscribe (chan, mycall, via, broker, servo_topic)

# Initialize PWM for controlling LED brightness.

GPIO.setwarnings(False) 

#r = 17
#g = 22
#b = 9

#GPIO.setmode(GPIO.BCM)		# GPIO number, not pin number
#GPIO.setup(r, GPIO.OUT)
#GPIO.setup(g, GPIO.OUT)
#GPIO.setup(b, GPIO.OUT)

r = 11
g = 15 
b = 21 

GPIO.setmode(GPIO.BOARD)		# RPi pin number
GPIO.setup(r, GPIO.OUT)
GPIO.setup(g, GPIO.OUT)
GPIO.setup(b, GPIO.OUT)

r_pwm = GPIO.PWM(r,100)
g_pwm = GPIO.PWM(g,100)
b_pwm = GPIO.PWM(b,100)

r_pwm.start(75)
g_pwm.start(75)
b_pwm.start(75)


def set_led (value):
    print ("********* set led to " + value)
    # Expecting the form like (9,9,9) with values 0-255.
    # This doesn't do any error checking so watch out.
    rgb = eval(value)	# Should be a tuple.
    (rr,gg,bb) = rgb
    r_pwm.ChangeDutyCycle((255-rr)*100/255)
    g_pwm.ChangeDutyCycle((255-gg)*100/255)
    b_pwm.ChangeDutyCycle((255-bb)*100/255)
    pass

def set_servo (value):
    print ("********* set servo to " + value)
    # Not done yet
    pass


# This function is called for any incoming messages.
# We need to decide if it is meant for us and act accordingly.

def on_msg (addressee,cmd,options,topic,value):
    #print ("Received message: " + addressee + ',' + cmd + ',' + topic + '=' + value)
    if addressee == mycall:
        if cmd == 'top':
            if topic == rgbled_topic:
                set_led (value)
            elif topic == servo_topic:
                set_servo (value)
            else:
                print ("Ignore - Unexpected topic.")     
        else:
            print ("Ignore - Not topic value message.")
            pass
    else:
        print ("Ignore - Message is for " + addressee + " but I am " + mycall)
        pass


# This is an infinite loop, watching for incoming messages.
# The specified 'on_msg' function is called when a message arrives.
# A "keep alive" is sent periodically so broker knows I'm
# still here so don't cancel the subscription.

hrot.recv_loop (chan,mycall,via,broker,on_msg)



