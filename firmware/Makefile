MICRO_TYPE?=esp32c6
BOARD?=minino

ifdef PORT
	BOARD_SERIAL=${PORT}
else
	BOARD_SERIAL=/dev/ttyACM0
endif


ifneq ($(BOARD), badge)
ifneq ($(BOARD), minino)
$(error Board not found: $(BOARD). Please use our supported boards: minino, badge)
endif
endif

ifdef OTA
ifneq ($(BOARD), minino)
$(error Board not support OTA yet)
endif
endif

.PHONY: compile load monitor build

ifeq ($(OTA), 1)
IDFPY_FLAGS += -B build_${BOARD}_ota @profiles/$(BOARD)_ota/profile
else
IDFPY_FLAGS += -B build_${BOARD} @profiles/$(BOARD)/profile
endif

define RUN_IDF_PY
	idf.py $(IDFPY_FLAGS) $(1)
endef

build:
	idf.py fullclean
	idf.py set-target $(MICRO_TYPE)
	$(call RUN_IDF_PY, build)

compile:
	$(call RUN_IDF_PY, build)

load:
	make compile BOARD=$(BOARD)
	$(call RUN_IDF_PY, -p $(PORT) flash)

monitor:
	$(call RUN_IDF_PY, -p $(PORT) monitor)