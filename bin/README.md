Summary of the scripts provided in this directory.

Setup Deel PC
=============

setup_devel_pc: installs linux packages, android sdk, env vars, and clones ezApp;
this has been tested on Ubuntu 26.04, and on Windows 11 WSL distro Ubuntu 26.04

install_android_sdk: install the Android Software Development Kit on the Devel PC;
this script is called by setup_devel_pc; this script can be re-run to upgrade to
a newer android sdk

Develop and Deploy miniApps & miniSvcs
======================================

For each of these, provide the '-h' option for help.
All of these, except eztest, require ezApp to be running on your smartphone,
with Devel_Mode enabled.

eztest: test build and run a miniApp or miniSvc on the Devel PC

ezsh: Simulates a shell running on the Android device. 

ezput: Copy miniApp and miniSvc files from Devel PC to Android device.

ezget: Copy miniApp and miniSvc files from Android device to Devel PC

ezbackup, ezrestore: backup and restore ezApp created data files

Android Debug Bridge Helper Scripts
===================================

adb_connect: establish a wireless debugging connection between your computer 
and your Android device; run 'adb_connect help' for details

adb_logcat, adb_logwatch, adb_logclr: monitor or clear the Android log

adb_meminfo, adb_top: display ezApp Android device resource utilization

adb_abi: displays the Android Device Application Binary Interface

adb_restart: kill and restart adb server; useful to resolve connection 
issue between the Devel PC and Android device

Other Scripts
=============

cscope_init: create cscope and tags database files

git_meld_diff: git is configured to run this script to view diffs

