# SPDX-License-Identifier: GPL-3.0-or-later
#
# VITA 49.2 packet subsystem, vendored from the esp-sdr project
# (ESPARGOS/esp-sdr, main/common/vrt_packet.c and iq_pack.c, GPL-3.0-or-later).
#
# These sources used to be compiled straight out of the esp-sdr checkout that
# tools/patches/apply_esp_sdr_minino.py used to fetch into <repo>/build-sdr.
# That tree is gitignored and the fetch script is gone, so the build silently
# degraded to an empty component (and the error message pointed at a script
# that no longer existed). The sources are vendored here instead: the build no
# longer depends on anything outside the repository.
idf_component_register(
    SRCS
        "iq_pack.c"
        "vrt_packet.c"
    INCLUDE_DIRS
        "."
)
