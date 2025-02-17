test_port:
	@if [ -z "$(PORT)" ]; then echo "PORT is not set, please set PORT using export, example: 'export PORT=/dev/ttyACM0'"; exit 1; fi

setup:
	idf.py set-target esp32c6
	pre-commit install

compile:
	idf.py build

flash:
	make test_port
	idf.py -p $(PORT) flash

monitor:
	make test_port
	idf.py -p $(PORT) monitor

clean:
	rm -rf build/ managed_components/
	rm sdkconfig
	make setup

ff:
	make test_port
	idf.py -p $(PORT) erase-flash
		make flash monitor

all: flash monitor

binmerge:
	idf.py build && cd build && esptool.py --chip esp32c6 merge_bin -o MininoBin.bin @flash_args

BOARDS = minino bSides dragonJar ekoParty bugCon

builds:
	$(foreach board, $(BOARDS), rm -rf build-$(board);)
	$(foreach board, $(BOARDS), $(call RUN_IDF_PY, -B build-$(board) @profiles/$(board)/profile, build);)

define RUN_IDF_PY
	idf.py $(1) $(2)
endef

.PHONY: test_port setup compile flash monitor clean all
