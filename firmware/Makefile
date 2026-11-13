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

FLASH_FIT_BOARDS_4MB = bsides dragonjar ekoparty bugcon bsseattle

flash_fit:
	@python3 tools/check_flash_fit.py --partition-table partitions.csv \
		--app-bin build-minino/minino.bin --label minino || exit 1; \
	for board in $(FLASH_FIT_BOARDS_4MB); do \
		python3 tools/check_flash_fit.py --partition-table partitions_4MB.csv \
			--app-bin build-$$board/minino.bin --label $$board || exit 1; \
	done

mergebins:
	@for board in $(BOARDS); do \
		(cd build-$$board && esptool.py --chip esp32c6 merge_bin -o $${board}Bin.bin @flash_args) || exit 1; \
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
		idf.py @profiles/$$board/profile build || exit 1; \
	done

# The SDR variant: the capture bank takes a whole SRAM block out of the heap
# and the image has to fit below it, which means the Zigbee and the Thread
# stacks stay out. See sdkconfig.sdr.
build-sdr:
	rm -rf build-sdr
	idf.py @profiles/sdr/profile build

tests:
	./tools/run_host_tests.sh

host-tests: tests

.PHONY: test_port setup compile flash monitor clean all ff binmerge flash_fit mergebins clean_builds builds build-sdr tests host-tests
