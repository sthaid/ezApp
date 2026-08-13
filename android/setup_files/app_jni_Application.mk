
# Uncomment this if you're using STL in your project
# You can find more information here:
# https://developer.android.com/ndk/guides/cpp-support
# APP_STL := c++_shared

APP_ABI := armeabi-v7a arm64-v8a x86 x86_64

# Min runtime API level
APP_PLATFORM=android-30  // EZAPP

# EZAPP
# - https://developer.android.com/guide/practices/page-sizes#update-packaging
# - Google AI says: 
#     "needed for Android apps that use native code (NDK) to be compatible with 
#      future Android devices running Android 15+ which use 16 KB memory page sizes."
APP_SUPPORT_FLEXIBLE_PAGE_SIZES := true
