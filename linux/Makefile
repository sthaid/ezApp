CMAKE_INSTALL_PREFIX = $(shell pwd)/local
CMAKE_PREFIX_PATH    = $(shell pwd)/local

build_all: SDL SDL_ttf SDL_mixer lodepng kissfft cJSON libmp3lame ezApp_lib picoc ezApp
build_quick: ezApp_lib picoc ezApp

ezApp:
	../src/ezApp/gen_version
	mkdir -p build/$@
	cd build/$@; cmake ../../../src/$@ -DCMAKE_PREFIX_PATH=$(CMAKE_PREFIX_PATH)
	cd build/$@; cmake --build . # --verbose

kissfft: CMAKE_MORE_OPTS = -DKISSFFT_STATIC=ON -DKISSFFT_TEST=OFF -DKISSFFT_TOOLS=OFF

SDL SDL_ttf SDL_mixer lodepng kissfft cJSON libmp3lame ezApp_lib picoc:
	mkdir -p build/$@
	cd build/$@; cmake  ../../../src/$@ -DCMAKE_INSTALL_PREFIX=$(CMAKE_INSTALL_PREFIX) $(CMAKE_MORE_OPTS)
	cd build/$@; cmake --build . # --verbose
	cd build/$@; cmake --install .

run: build_quick
	cd ../files; ../linux/build/ezApp/ezApp 2>&1 | ../bin/src/ezlogfilter

clean:
	rm -f  ../src/ezApp/version.h
	rm -rf build local

.PHONY: SDL SDL_ttf SDL_mixer lodepng kissfft cJSON libmp3lame ezApp_lib picoc
.PHONY: ezApp run build_all build_quick clean

