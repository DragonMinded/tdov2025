# Please see the README for setting up a valid build environment.

# The top-level binary that you wish to produce.
all: tdov.bin

# All of the source files (.c and .s) that you want to compile.
# You can use relative directories here as well. Note that if
# one of these files is not found, make will complain about a
# missing missing `build/naomi.bin' target, so make sure all of
# these files exist.
SRCS += main.c

# Make sure to link with our font and sound libs (from libnaomi 3rdparty).
LIBS += -lxmp -lfreetype -lbz2 -lz -lpng16

# Unfortunately, libvorbis has warnings generated out of its headers.
FLAGS += -Wno-unused-variable -Wall

# We want a different serial for whatever reason.
SERIAL = BTV0

# Pick up base makefile rules common to all examples.
include ${NAOMI_BASE}/tools/Makefile.base

# Provide a rule to build our ROM FS.
build/romfs.bin: romfs/ ${ROMFSGEN_FILE}
	mkdir -p romfs/
	${ROMFSGEN} $@ $<

# Provide the top-level ROM creation target for this binary.
# See scripts/makerom.py for details about what is customizable.
tdov.bin: ${MAKEROM_FILE} ${NAOMI_BIN_FILE} build/romfs.bin
	${MAKEROM} $@ \
		--title "Trans Day of Visibility" \
		--publisher "DragonMinded" \
		--serial "${SERIAL}" \
		--section ${NAOMI_BIN_FILE},${START_ADDR} \
		--entrypoint ${MAIN_ADDR} \
		--main-binary-includes-test-binary \
		--test-entrypoint ${TEST_ADDR} \
		--align-before-data 4 \
		--filedata build/romfs.bin

copy: tdov.bin
	mkdir -p release
	cp tdov.bin release/tdov2025.bin

# Include a simple clean target which wipes the build directory
# and kills any binary built.
.PHONY: clean
clean:
	rm -rf build
	rm -rf tdov.bin
