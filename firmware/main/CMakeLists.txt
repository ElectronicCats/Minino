file(GLOB_RECURSE ALL_SOURCE_FILES "./*.c" "./*.cpp")
set(modules_src "")

foreach(file ${ALL_SOURCE_FILES})
    if(NOT file MATCHES "/test/")
        list(APPEND modules_src ${file})
    endif()
endforeach()

# Build variants: the apps of a subsystem that the variant leaves out must not
# be compiled at all. "main" globs the whole tree, so the sources are filtered
# here; the components themselves register empty (see cmake/minino_kconfig.cmake
# for why the option is read from the sdkconfig and not from CONFIG_*).
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_BT_STACK _minino_bt_on)
minino_kconfig_enabled(MININO_ZIGBEE_STACK _minino_zigbee_on)
minino_kconfig_enabled(MININO_OPENTHREAD_STACK _minino_openthread_on)

if(NOT _minino_bt_on)
    list(FILTER modules_src EXCLUDE REGEX "/apps/ble/")
    # The drone_id component needs the Bluetooth stack, and so do the apps
    # that use it.
    list(FILTER modules_src EXCLUDE REGEX "/apps/wifi/drone_id/")
    list(FILTER modules_src EXCLUDE REGEX "/apps/wifi/droneid_scanner/")
endif()
if(NOT _minino_zigbee_on)
    list(FILTER modules_src EXCLUDE REGEX "/apps/zigbee/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/zigbee/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/war_bee/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/settings/zb_cli_settings/")
endif()
if(NOT _minino_openthread_on)
    list(FILTER modules_src EXCLUDE REGEX "/apps/thread_sniffer/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/open_thread/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/war_thread/")
endif()

file(GLOB_RECURSE ALL_INCLUDE_PATHS "./*")
set(headers_dirs "")

foreach(path ${ALL_INCLUDE_PATHS})
    get_filename_component(parentDir ${path} DIRECTORY)

    if(IS_DIRECTORY ${parentDir} AND NOT parentDir MATCHES "/test")
        list(APPEND headers_dirs ${parentDir})
    endif()
endforeach()

idf_component_register(
                    SRCS
                    "${modules_src}"
                    EMBED_FILES modules/web_file_browser/http_server/header.html modules/web_file_browser/http_server/favicon_b.ico apps/wifi/captive/root.html
                    apps/wifi/captive/redirect.html
                    modules/web_file_browser/http_server/style.css
                    INCLUDE_DIRS
                    "${headers_dirs}")
# NOTE: do NOT add a REQUIRES/PRIV_REQUIRES list here. "main" globs sources from
# the whole app tree and relies on IDF's "requires every other component"
# default; naming a single requirement turns that off and every other component
# (ble_scann, preferences, nvs_flash, ...) drops out of the include path.
#
# The certification-test archive (librftest.a) is not linked.
#
# Its tone (wifiscwout) needs the whole cert-test state, and the object
# that sets that state up (wifi.o) drags in mac_common.o with its 45 KB of
# static IQ buffers - more than a quarter of what the capture bank leaves
# free. The C6 has no transmitter anyway: the dump engine reads the RF
# front end and there is no DAC to write samples to, so the tone is not
# linked in and the SDR app is receive-only.
#
# SDR build variant: the C6 dump engine needs the whole of SRAM block 2 as
# the capture bank, so the whole DRAM image must fit below it. The extra
# linker script redefines LENGTH(sram_seg) to the block boundary, which turns
# any overlap into a hard link error ("region sram_seg overflowed") instead
# of silent corruption at capture time. esp_sdr_memory.c reserves the bank
# from the heap via the SOC_RESERVE_MEMORY_REGION mechanism.
#
# The whole firmware (Bluetooth, Zigbee, Thread, the surveillance apps) fits
# below the bank: the image is 256 KB of IRAM+data+bss and the bank is the
# 128 KB above it. The linker script fails the build if that ever stops being
# true, with the size of the overflow, instead of corrupting the heap at
# capture time.
#
# CONFIG_SDR_ENABLE is read here (and not through minino_kconfig.cmake) on
# purpose: the link options only matter in the second pass of this file, the
# one that configures the ELF target.
if(CONFIG_SDR_ENABLE)
    message(STATUS "SDR: capture bank cap ENABLED")
    idf_build_set_property(LINK_OPTIONS "-T" APPEND)
    idf_build_set_property(LINK_OPTIONS
                           "${CMAKE_CURRENT_LIST_DIR}/esp_sdr_bank.ld" APPEND)
endif()
