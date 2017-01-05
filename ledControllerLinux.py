import struct
import time
import pycurl
import serial
from StringIO import StringIO
import json

MODE_BLINK = 0
MODE_THEATER = 1
MODE_STATIC = 2
MODE_TWINKLE = 3
AUTOMATIC_MODE = 100
BRIGHTNESS_LOW = 101
BRIGHTNESS_MEDIUM = 102
BRIGHTNESS_HIGH = 103
TOGGLE_BLUE = 104
BLUE_MODE = 105
RED_MODE = 106
WHITE_MODE = 107

def sendCmd( cmd ):
  print("Sending Command {}".format(cmd))
  data = struct.pack("<B", cmd)
  Serial4.write(data)
  time.sleep(2)

def getLastCommitHash(repo):
	buffer = StringIO()
	c = pycurl.Curl()
	c.setopt(c.URL, 'https://api.bitbucket.org/2.0/repositories/REPO/{}/commits'.format(repo))
	c.setopt(c.WRITEDATA, buffer)
	c.setopt(pycurl.USERPWD, '%s:%s' % ("USERNAME", "PASSWORD"))
	c.perform()
	c.close()
	try:
		j = json.loads(buffer.getvalue())
		return j["values"][0]["hash"]
	except ValueError:
		print "Value Error"
		print buffer.getvalue()
		return -1

	return -1

def getCombinedHash():
	vscHash = getLastCommitHash("uvsc")
	oipHash = getLastCommitHash("oip_scm")

	if (vscHash != -1) and (oipHash != -1):
		return getLastCommitHash("uvsc")+getLastCommitHash("oip_scm")

	return -1


#UART.setup("UART4")
Serial4 = serial.Serial(port = "/dev/ttyUSB0", baudrate=9600)
Serial4.close()
Serial4.open()

lastCommitHash = getCombinedHash()

sendCmd(WHITE_MODE)
sendCmd(BRIGHTNESS_MEDIUM)
sendCmd(MODE_TWINKLE)

while True:
	h = getCombinedHash()
	if h != -1:
		print h
		if h != lastCommitHash:
			print("New Hash!")
			lastCommitHash = h
			sendCmd(MODE_BLINK)
			time.sleep(5)
			sendCmd(MODE_THEATER)
			time.sleep(5)
			sendCmd(MODE_TWINKLE)
		#	sendCmd(TOGGLE_BLUE)
	else:
		print "Error getting hash"

	time.sleep(30)



  

  
