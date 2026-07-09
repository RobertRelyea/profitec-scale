# Makefile for profitec-scale
# Builds target executables for both Raspberry Pi Pico (RP2040) and Pico 2 (RP2350).

.PHONY: all pico1 pico2 clean

all: pico1 pico2

pico1: build_pico1/CMakeCache.txt
	cmake --build build_pico1 -j$$(nproc 2>/dev/null || echo 4)

pico2: build_pico2/CMakeCache.txt
	cmake --build build_pico2 -j$$(nproc 2>/dev/null || echo 4)

build_pico1/CMakeCache.txt:
	cmake -B build_pico1 -DPICO_BOARD=pico -DCMAKE_BUILD_TYPE=Release

build_pico2/CMakeCache.txt:
	cmake -B build_pico2 -DPICO_BOARD=pico2 -DCMAKE_BUILD_TYPE=Release

clean:
	rm -rf build_pico1 build_pico2
