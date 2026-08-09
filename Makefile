# --- build ---

APPS := $(sort $(dir $(wildcard files/apps/*/.)))
SVCS := $(sort $(dir $(wildcard files/svcs/*/.)))

build: clone_sdl bin/src linux test_build_apps_and_svcs

# the SDL checkouts are from Sep 13, 2026
clone_sdl:
	@if [ ! -d src/SDL -o ! -d src/SDL_ttf -o ! -d src/SDL_mixer ]; then \
            SRC=$$PWD/src; \
            cd $$SRC; \
            echo "remove existing clones"; \
            rm -rf SDL SDL_ttf SDL_mixer; \
            echo "clone repos"; \
            git clone https://github.com/libsdl-org/SDL; \
            git clone https://github.com/libsdl-org/SDL_ttf; \
            git clone https://github.com/libsdl-org/SDL_mixer; \
            echo "checkout branches"; \
            cd $$SRC/SDL;       git checkout -q 33b4a9d915947d2482366622fc4f97c944254775; \
            cd $$SRC/SDL_ttf;   git checkout -q 65df5b20d7f6497f24cdf78e583205d53e5c96a1; \
            cd $$SRC/SDL_mixer; git checkout -q 9edf53e092202ecc41118057d73eca6ad4ea1138; \
            echo "download external repos"; \
            cd $$SRC/SDL_ttf/external;   ./download.sh; \
            cd $$SRC/SDL_mixer/external; ./download.sh; \
        fi

bin/src:
	make -C bin/src

linux:
	make -C linux

test_build_apps_and_svcs:
	for d in $(APPS) ; do echo "\n======== BUILD APP $$d ========\n"; cd $$d; eztest build || exit 1; cd ../../..; done
	for d in $(SVCS) ; do echo "\n======== BUILD SVC $$d ========\n"; cd $$d; eztest build || exit 1; cd ../../..; done

.PHONY: build clone_sdl bin/src linux test_build_apps_and_svcs 

# --- android build & install  ---

build_android: 
	make -C android build

install_android: 
	make -C android install

build_and_install_android: 
	make -C android build_and_install

.PHONY: android_build android_install

# --- clean ---

clean:
	git clean -fdx
	rm -rf src/SDL src/SDL_mixer src/SDL_ttf android/SDL
	@echo "Remaining files:"; git ls-files --other

.PHONY: clean
