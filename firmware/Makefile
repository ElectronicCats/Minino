test_port:
	@if [ -z "$(PORT)" ]; then echo "PORT is not set, please set PORT using export, example: 'export PORT=/dev/ttyACM0'"; exit 1; fi

setup:
	idf.py set-target esp32c6

compile:
	idf.py build

flash:
	make test_port
	idf.py -p $(PORT) flash

monitor:
	make test_port
	idf.py -p $(PORT) monitor

clean:
	-rm -rf build/ managed_components/
	-rm sdkconfig
	-rm dependencies.lock
	-make setup

ff:
	make test_port
	idf.py -p $(PORT) erase-flash
	make flash monitor

all: flash monitor


binmerge:
	cd build-minino && esptool.py --chip esp32c6 merge_bin -o MininoBin.bin @flash_args

BOARDS = minino bsides dragonjar ekoparty bugcon bsseattle

mergebins:
	@for board in $(BOARDS); do \
		cd build-$$board && esptool.py --chip esp32c6 merge_bin -o $$board`Bin.bin` @flash_args && cd ..; \
	done

clean_builds:
	@for board in $(BOARDS); do \
		rm -rf build-$$board; \
	done

builds:
	@for board in $(BOARDS); do \
		rm -rf build-$$board; \
	done
	@for board in $(BOARDS); do \
		idf.py @profiles/$$board/profile build; \
	done

.PHONY: test_port setup compile flash monitor clean all ff binmerge mergebins clean_builds builds
