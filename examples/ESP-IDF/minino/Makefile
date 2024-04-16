PORT := /dev/cu.usbmodem132101

setup:
	idf.py set-target esp32c6
	pre-commit install

compile:
	idf.py build

flash:
	idf.py -p $(PORT) flash

monitor:
	idf.py -p $(PORT) monitor

clean:
	rm -rf build/ managed_components/
	rm sdkconfig sdkconfig.old

all: compile flash monitor

.PHONY: setup compile flash monitor clean all
