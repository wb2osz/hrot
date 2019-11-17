#!/usr/bin/env python3

# "Ham Radio of Things" example.
# WB2OSZ, November 2019

# Use this as a starting point for your own application.

# Read from DHT22 Temperature-Humdity sensor and "publish" the values.

# This is derived from https://learn.adafruit.com/dht-humidity-sensing-on-raspberry-pi-with-gdocs-logging/python-setup

import time
import board
import adafruit_dht

import hrot

# Apply your customizations here.

chan = 0		# Radio channel for multi-port TNC.

mycall = 'WB2OSZ-1'	# Name of my "thing" node.  
			# >>>>>> CHANGE THIS to use your own call. <<<<<<

broker = 'WB2OSZ-15'	# Station ID for the Broker.  
			# Could be a shared community resource.
via = 'WIDE1-1'


# Initialize the dht device, with data pin connected to.
# Change this is you are using some pin other than D4.
dhtDevice = adafruit_dht.DHT22(board.D4)

while True:
    try:
        temperature_c = dhtDevice.temperature
        temperature_f = temperature_c * (9 / 5) + 32
        humidity = dhtDevice.humidity
        print("Temp: {:.1f} F / {:.1f} C    Humidity: {}% "
              .format(temperature_f, temperature_c, humidity))

        hrot.publish (chan, mycall, via, broker, 'temperature', "{:.1f}".format(temperature_f))
        #hrot.publish (chan, mycall, via, broker, 'temperature', "{:.1f}".format(temperature_c))
        hrot.publish (chan, mycall, via, broker, 'humidity', "{:.0f}".format(humidity))

    except RuntimeError as error:
        # Errors happen fairly often, DHT's are hard to read, just keep going
        print(error.args[0])

    time.sleep(15.0)
