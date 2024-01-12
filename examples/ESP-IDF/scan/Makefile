PORT := /dev/cu.usbserial-550D0100521

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