TOOLCHAIN_PATH=$(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/linux-x86_64/bin

ifeq ($(wildcard $(TOOLCHAIN_PATH)/.),)
    $(error Error: The directory '$(TOOLCHAIN_PATH)' does not exist)
endif

export PATH := $(TOOLCHAIN_PATH):$(PATH)

# note - commit of openssl being used is:
#   commit d099e33e5733bb9d3975fc4f3ac4a85b6ed1a4cb
#   Author: 007bsd <22483432+007bsd@users.noreply.github.com>
#   Date:   Tue May 26 00:10:43 2026 +0300
#       aes_wrap: prevent crash on update without a key
COMMIT=d099e33e5733bb9d3975fc4f3ac4a85b6ed1a4cb

build: libs/arm64-v8a/libcrypto.so

libs/arm64-v8a/libcrypto.so:
	# clone and build openssl
	( cd /tmp; \
          rm -rf openssl; \
          git clone https://github.com/openssl/openssl; \
          cd openssl; \
          git checkout $(COMMIT); \
          ./Configure android-arm64 shared; \
          make )
	# populate the openssl dir from the openssl build
	rm -rf include libs
	mkdir -p include libs/arm64-v8a
	cp -r /tmp/openssl/include/openssl include
	cp  /tmp/openssl/libcrypto.so libs/arm64-v8a
	cp  /tmp/openssl/libssl.so libs/arm64-v8a
	# cleanup /tmp
	rm -rf /tmp/openssl

clean:
	rm -rf libs include
