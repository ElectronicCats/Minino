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
	cd build-minino && esptool.py --chip esp32c6 merge_bin -o MininoBin.bin @flash_args

BOARDS = minino

mergebins:
	$(foreach board, $(BOARDS), cd build-$(board) && esptool.py --chip esp32c6 merge_bin -o $(board)Bin.bin @flash_args; cd ..;)

clean_builds:
	$(foreach board, $(BOARDS), rm -rf build-$(board);)

builds:
	$(foreach board, $(BOARDS), rm -rf build-$(board);)
	$(foreach board, $(BOARDS), $(call RUN_IDF_PY, @profiles/$(board)/profile, build);)

define RUN_IDF_PY
	idf.py $(1) $(2)
endef

.PHONY: test_port setup compile flash monitor clean all
