BOARD_TAG = esp32:esp32:esp32s3:CDCOnBoot=cdc
MONITOR_PORT = /dev/cu.usbmodem101

compile:
	arduino-cli compile --fqbn $(BOARD_TAG)

upload:
	arduino-cli upload -p $(MONITOR_PORT) --fqbn $(BOARD_TAG)

monitor:
	arduino-cli monitor -p $(MONITOR_PORT) --fqbn $(BOARD_TAG)

clean:
	arduino-cli cache clean

all: compile upload monitor