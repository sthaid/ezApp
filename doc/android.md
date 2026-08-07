Build & Install ezApp on your Android Device
============================================

Prior to using this document; the steps described in miniApps.md, 
"Setup Ubuntu Devel PC" or "Setup Windows Devel PC" must have
been performed.

Those steps will have installed the Android SDK, in ~/android/sdk.
And also set the required environment variables, in ~/.bashrc_ezApp.

Table of Contents
=================

- Connect adb with your Android Device
- Build ezApp Android APK
- Install ezApp Android APK
- Appendix A - adb Commands
- Appendix B - Android Version Info
- Appendix C - Miscellaneous Notes

Connect adb with your Android Device
====================================

Run ```adb_connect help``` and follow the instructions to:
- enable Android Developer options
- establish a Wireless Debugging Connection from your Devel PC to your Android device.

Build ezApp Android APK
=======================

This will take a few minutes on the first build.
Subsequent builds will ususally be faster.
```
cd ~/ezApp/android
make build
```

Install ezApp Android APK
=========================

```
cd ~/ezApp/android
make install
```

Appendix A - adb Commands
=========================

xxx check indenting here

General
- adb help:                show help
- adb kill-server; adb start-server:
                           restart Android Debug Bridge (ADB) daemon on Devel PC

Connecting Devel PC with Android 
- adb pair HOST:[PORT]:    pair Devel PC with Android device
- adb connect HOST[:PORT]: connect Devel PC with Android device, using Wi-Fi TCP/IP
- adb disconnect:          disconnect from all devices
- adb devices:             list connected devices

xxx todo others

Appendix B - Android Version Info
=================================

Reference: https://apilevels.com/

Android Version found here:: Setup -> About phone -> Software information. 
Some examples:
```
                    Android   
    Codename        Version     SDK / API Level   Year
    --------        -------     ---------------   ----
    Baklava           16             36           2025
    Nougat            7.0            24           2016
    Lollipop          5              21           2015
```

From Google AI ...

Android SDK Version (minSdkVersion, targetSdkVersion)
- These are attributes used in your app's main build configuration 
  (like a Gradle file in Android Studio) to declare compatibility
  with specific API levels. 
- minSdkVersion: The minimum API level your application can run on.
  Devices running an Android version with an API level lower than your
  minSdkVersion will not be able to install your app from Google Play.
- targetSdkVersion: The API level your app was tested against and is 
  fully compatible with.
- compileSdkVersion: The API level of the Android platform version you
  compile your app against. You should always use the latest stable 
  version for this to access new features and improvements. 

APP_PLATFORM 
- a variable used specifically within the Android NDK (Native Development
  Kit) build system (primarily in the Application.mk file for ndk-build). 
- It declares the Android API level against which your native C/C++ code
  is compiled. For native code, the APP_PLATFORM essentially acts as the
  minimum required API level for your native libraries, similar to how
  minSdkVersion works for the overall application.
- If not specified in Application.mk, ndk-build typically defaults to a
  minimum API level supported by the NDK itself or tries to infer it from
  your minSdkVersion set in the app's manifest/Gradle file. 

NDK Version:
- explicit configuration, in build.gradle:
        android {
            ...
            ndkVersion "23.1.7779620"
        }
- Automatic Selection:
  If you do not specify an ndkVersion, the Android Gradle Plugin (AGP)
  will automatically select a default version that it is known to be
  compatible with

Appendix C - Miscellaneous Notes
================================

* To list what files are contained in the APK:
```
    cd ~/ezApp/android
    unzip -l SDL/build/org.sthaid.ezApp/app/build/outputs/apk/debug/app-debug.apk
    apkanalyzer files list SDL/build/org.sthaid.ezApp/app/build/outputs/apk/debug/app-debug.apk
    apkanalyzer files list SDL/build/org.sthaid.ezApp/app/build/outputs/apk/debug/app-debug.apk | grep lib
```

* Virtually all modern Android devices run on ARM64 architecture.

* gradle requires a Java Development Kit (JDK) or Java Runtime Environment 
  JRE) of version 17 or higher to run its engine

