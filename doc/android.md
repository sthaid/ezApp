Developer build and install ezApp on your Android device
========================================================

This document describes how to:
- install the Android SDK on your Devel PC
- enable Developer Options on your Android device
- build ezApp on your Devel PC
- install your ezApp build on your Android device

Prior to using the steps described in this document; the
steps described in miniApps.md, "Setup Development PC" must
have been performed.

Install the Android SDK on your Devel PC
========================================

Download the Linux Android SDK command line tools zip file.
from https://developer.android.com/studio#command-line-tools-only.

Run ezApp/bin/install_android_sdk script to install the Android SDK.
The SDK will be created in ~/android/sdk.
```
install_android_sdk ~/Downloads/commandlinetools-linux-nnnnnnnn_latest.zip
```
Setup the enviroment variables as described by the ```install_android_sdk``` script.

Enable Developer Options on your Android device
===============================================

On Android device:
- Enable developer mode by tapping "Build number" 8 times.
  - Settings -> About Phone -> Software Information -> Build Number
- Go to Settings > Developer options , and enable
  - USB Debugging
  - Stay Awake

Establish connection using USB **Data** cable. 
- Connect USB **Data** cable from Devel PC to Android device
- When prompted on Android device, tap:
  - Always allow from this computer
  - Allow
- Test connection, from Devel PC ```adb shell```

Optional additional steps to connect using Wi-Fi:
- On Devel PC ```adb tcpip 5555```
- Disconnect USB Data cable
- Get IP address of Android device, either from:
  - Android Settings > About Phone > Status information
  - ezApp Settings
- On Devel PC: ```adb connect <ipaddr>```
- Test Wi-Fi connection ```adb shell```

Notes:
- It is recommended to use the optional Wi-Fi procedure only on a trusted network.
- Use ```ezApp/bin/adb_connect <ipaddr>``` instead of ```adb connect <ipaddr>```
  to resolve connection issues. Refer to comments in that script.
- You will occasionally need to re-issue  ```ezApp/bin/adb_connect <ipaddr>```.
- On home network, recommend adjusting router setting to use static IP address for your
  Android device.

Summary of adb commands related to connecting to your Android device:
- adb help:                show help
- adb tcpip PORT           restart ADB daemon listening on TCP on PORT
- adb connect HOST[:PORT]: connect to a device via TCP/IP [default port=5555]
- adb disconnect:          disconnect from all devices
- adb devices:             list connected devices
- adb kill-server; adb start-server: 
                           restart Android Debug Bridge (ADB) daemon on Devel PC

Build ezApp on your Devel PC
============================

```
cd ~/ezApp/android
make build
```

Install your ezApp build on your Android device
===============================================

```
cd ~/ezApp/android
make install
```

APPENDICES
==========

android version info
--------------------

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

miscellaneous notes
-------------------

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

