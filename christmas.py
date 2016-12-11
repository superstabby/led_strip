# serial_echo.py - Alexander Hiam - 4/15/12
# 
# Prints all incoming data on Serial2 and echos it back.
# 
# Serial2 TX = pin 21 on P9 header
# Serial2 RX = pin 22 on P9 header
# 
# This example is in the public domain

from bbio import *
import struct
import time
import subprocess

MODE_FADE = 0
MODE_BLINK = 1
MODE_THEATER = 2
MODE_STATIC = 3
MODE_TWINKLE = 4
AUTOMATIC_MODE = 100
BRIGHTNESS_LOW = 101
BRIGHTNESS_MEDIUM = 102
BRIGHTNESS_HIGH = 103
TOGGLE_BLUE = 104
BLUE_MODE = 105
RED_MODE = 106

def sendCmd( cmd ):
  data = struct.pack("<B", cmd)
  Serial4.write(data)

def getLastRev():
	return subprocess.Popen(["./getlastrev.sh"], stdout=subprocess.PIPE, shell=True).communicate()[0]

def pull():
	subprocess.call(["git --git-dir uvsc/.git pull"], shell=True)

Serial4.begin(9600)

lastCommitHash = getLastRev()

sendCmd(RED_MODE)

while True:
	pull()
	h = getLastRev()
	if h != lastCommitHash:
		print("New Hash!")
		lastCommitHash = h
		sendCmd(MODE_BLINK)
		time.sleep(5)
		sendCmd(MODE_THEATER)
		time.sleep(5)
		sendCmd(MODE_TWINKLE)
		sendCmd(TOGGLE_BLUE)

	time.sleep(5)



  

  
