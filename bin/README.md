This document provides an overview of the scripts provided in this directory.

Develop and Deploy miniApps & miniSvcs
======================================

For each of these, provide the '-h' option for help.

ezsh: Simulates a shell running on the Android device. 

ezput: Copy miniApp and miniSvc files from Devel PC to Android device.

ezget: Copy miniApp and miniSvc files from Android device to Devel PC

eztest: test build and run a miniApp or miniSvc on the Devel PC

Install the Android SDK
=======================

install_android_sdk: install the Android Software Development Kit on the Devel PC

Build and Test ezApp
====================

Prior to using these scripts 'Developer options' must be enabled on the Android Device.
Refer to ezApp/doc/android.md for instructions.

adb_connect: establish a wireless debugging connection between your computer 
and your Android device; this needs to be re-done occasionally after having 
enabled Android device 'Developer options'

adb_logcat, adb_logwatch, adb_logclr: monitor or clear the Android log

adb_meminfo, adb_top: display ezApp Android device resource utilization

adb_abi: displays the Android Device Application Binary Interface

Other Scripts
=============

cscope_init: create cscope and tags database files
