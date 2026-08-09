# --- build ---

APPS := $(sort $(dir $(wildcard files/apps/*/.)))
APPS := $(filter-out files/apps/lib/, $(APPS))
SVCS := $(sort $(dir $(wildcard files/svcs/*/.)))

build: clone_sdl bin/src linux test_build_apps_and_svcs

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
            cd $$SRC/SDL;       git checkout -q 616b8fb338c55249a4a9d6c2245f0a10410f9aec; \
            cd $$SRC/SDL_ttf;   git checkout -q a42434b8c96daaf7650dbd0befe480c090d1c2eb; \
            cd $$SRC/SDL_mixer; git checkout -q 9c12eb245ebbf568b6eb7b838cf350aa9f729497; \
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
