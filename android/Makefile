ifeq ($(strip $(ANDROID_HOME)),)
    $(error ANDROID_HOME environment variable is not defined or is empty)
endif
ifeq ($(wildcard $(ANDROID_HOME)/.),)
    $(error Required directory '$(ANDROID_HOME)' does not exist)
endif
ifeq ($(findstring $(ANDROID_HOME),$(PATH)),)
    $(error ANDROID_HOME is not present in PATH environment variable)
endif

ifeq ($(strip $(ANDROID_NDK_ROOT)),)
    $(error ANDROID_NDK_ROOT environment variable is not defined or is empty)
endif
ifeq ($(wildcard $(ANDROID_NDK_ROOT)/.),)
    $(error Required directory '$(ANDROID_NDK_ROOT)' does not exist)
endif

build: SDL gen_version gen_assets openssl
	cd SDL/build/org.sthaid.ezApp; \
        ./gradlew assembleDebug

install:
	# -r: reinstall existing app and keep its data
	adb devices
	cd SDL/build/org.sthaid.ezApp; \
        adb install -r ./app/build/outputs/apk/debug/app-debug.apk

build_and_install: build install

uninstall:
	@echo "Android ezApp data files will be lost."
	@echo "Consider saving and restoring the data files using ezbackup and ezrestore"
	@read -p "Continue (y/n)? " yn; \
	if [ "$$yn" = "y" ]; then \
	    adb uninstall org.sthaid.ezApp; \
	else \
	    echo "CANCELLED"; \
	fi

.PHONY: build install build_and_install uninstall

# --------------------

SDL:
	./setup

gen_version:
	../src/ezApp/gen_version

gen_assets:
	# create files.tar
	cd ../files; \
	git ls-files > /tmp/files.txt; \
	tar -cf ../android/assets/files.tar --dereference --files-from /tmp/files.txt; \
	rm -f /tmp/files.txt
	# create files.tar.sig
	date > assets/files.tar.sig

openssl:
	make -C ../src/openssl

.PHONY: gen_version gen_assets openssl

# --------------------

clean:
	rm -rf SDL 
	rm -rf assets 
	rm -rf setup_files/orig
	rm -f  ../src/ezApp/version.h
	make -C ../src/openssl clean

.PHONY: clean
