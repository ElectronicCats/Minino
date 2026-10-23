# SPDX-License-Identifier: GPL-3.0-or-later
#
# Read a boolean Kconfig option from the configuration the build uses.
#
# Why not just if(CONFIG_FOO)? ESP-IDF evaluates every component's
# CMakeLists.txt while the component tree is expanded, and that happens
# before the Kconfig step on a fresh build: no CONFIG_* variable exists yet.
# A plain if(CONFIG_FOO) therefore takes the false branch, and the component
# ends up registered without its sources or its requirements - the build then
# fails with "bt_gattc.h includes esp_bt.h ... not in the requirements list of
# bt_gattc" or with empty components.
#
# Two sources are read, in this order:
#
#  1. The sdkconfig of the build (SDKCONFIG). It holds the resolved value of
#     every option, but on a fresh build it does not exist yet: it is written
#     by the Kconfig step, which runs after the component tree is expanded.
#
#  2. The chain of defaults files (SDKCONFIG_DEFAULTS), which always exists
#     because the build passes it on the command line. The options this is
#     used for are pinned there, so the answer is the same in both passes.
#
# The last file of the chain wins, the same way kconfgen resolves them.
#
# The options this is used for are Minino's own (MININO_BT_STACK,
# MININO_ZIGBEE_STACK, MININO_OPENTHREAD_STACK, SDR_ENABLE) and not ESP-IDF's
# own symbols: an ESP-IDF symbol is not in the defaults files, so the helper
# would not find it on a fresh build.
#
# Usage:
#   include("${CMAKE_SOURCE_DIR}/cmake/minino_kconfig.cmake")
#   minino_kconfig_enabled(MININO_BT_STACK _bt_on)
#   if(_bt_on)
#       idf_component_register(SRCS ...)
#   else()
#       idf_component_register()
#   endif()

# Reads one option out of a configuration file.
#   1  the option is on      ("CONFIG_X=y")
#   0  the option is off     ("CONFIG_X=n" or "# CONFIG_X is not set")
#  -1  the file does not mention it
function(_minino_option_state FILE OPTION OUT)
    # The result is only written when the file mentions the option: a file
    # that does not talk about it must not wipe what another file said.
    if(NOT EXISTS "${FILE}")
        return()
    endif()
    file(STRINGS "${FILE}" _hits REGEX "^(# )?CONFIG_${OPTION}(=| is not set)")
    foreach(_hit IN LISTS _hits)
        if(_hit MATCHES "=y$")
            set(${OUT} 1 PARENT_SCOPE)
            return()
        elseif(_hit MATCHES "=n$" OR _hit MATCHES "is not set")
            set(${OUT} 0 PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

function(minino_kconfig_enabled OPTION RESULT)
    set(${RESULT} 0 PARENT_SCOPE)
    set(_state -1)

    # The resolved configuration, when the build has one already.
    if(DEFINED SDKCONFIG AND EXISTS "${SDKCONFIG}")
        _minino_option_state("${SDKCONFIG}" ${OPTION} _state)
        if(NOT _state EQUAL -1)
            set(${RESULT} ${_state} PARENT_SCOPE)
            return()
        endif()
    endif()

    # The chain of defaults, from the first file to the last one: the last
    # file that mentions the option wins, the way kconfgen resolves them.
    # The paths come from the command line and are relative to the project
    # directory, which is where a component's own CMakeLists resolves them
    # to something else entirely.
    if(DEFINED SDKCONFIG_DEFAULTS)
        foreach(_file IN LISTS SDKCONFIG_DEFAULTS)
            if(NOT IS_ABSOLUTE "${_file}")
                set(_file "${CMAKE_SOURCE_DIR}/${_file}")
            endif()
            _minino_option_state("${_file}" ${OPTION} _state)
        endforeach()
    endif()

    if(_state EQUAL 1)
        set(${RESULT} 1 PARENT_SCOPE)
    endif()
endfunction()
