PORT := /dev/cu.usbmodem11301

compile:
	idf.py build

flash:
	idf.py -p $(PORT) flash

monitor:
	idf.py -p $(PORT) monitor

clean:
	idf.py fullclean

all: compile flash monitor

.PHONY: compile flash monitor clean all