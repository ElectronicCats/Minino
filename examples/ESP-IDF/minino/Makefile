PORT := /dev/cu.usbmodem1132101

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

.PHONY: compile flash monitor clean all