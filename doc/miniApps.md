Create a miniApp
================

This document describes how to:
- install and setup ezApp on your Android Smartphone
- create a new miniApp using a Development PC
- transfer the new miniApp from the Development PC to the ezApp
- run the new miniApp on your Android Smartphone

Install ezApp on your Android device
====================================

Install ezApp from the Google Play Store.
xxx todo publish on Google Play Store

Open ezApp, accept the permission requests, and try out the miniApps that
are included with ezApp. Select Settings > Readme for a brief description of the
miniApps that are included.

If you want to create a new miniApp then continue with the steps described in this document.

Enable ezApp Developer Mode.
- Open ezApp, 'Settings' selection should be at bottom right of display
- Settings > Devel_Mode > Tap to set ON
- Settings > Devel_Password > Tap to set your password

You do not need to enable Android Device 'Developer options' to create miniApps.

Setup Ubuntu Devel PC
====================

xxx update this section

A PC running Windows 11 or Ubuntu 25.10 (or newer) is required.
Other Linux distros may also work.

Install Ubuntu Linux packages:
```
sudo apt update
sudo apt install -y git
sudo apt install -y build-essential cmake
sudo apt install -y openjdk-17-jdk-headless
sudo apt install -y libssl-dev libreadline-dev libasound2-dev
sudo apt install -y cscope universal-ctags
```

Setup Windows Devel PC
======================

Using wsl ...
wsl --install                     # install wsl; NOT reboot required 'shutdown /r /t 0'
wsl --status                      # should inidcate Defaut Version: 2
wsl --list --online               # lists avail distros
wsl --install -d Ubuntu-26.04     # install wsl support for Ubuntu-26.04; respond to queries
wsl --list --verbose              # lists installed distros, should show Ubuntu-26.04
wsl --set-default Ubuntu-26.04    # this is optional, Ubuntu-26.04 should already be the default
wsl ~                             # starts bash on the default distro, and cd to wsl home dir
wsl ~ -u root -d Ubuntu-26.04     # starts bash for user root in distro Ubuntu-26.04

wsl --help                        # display usage
wsl --shutdown                    # terminates all running distros
wsl --update                      # updates wsl
wsl --version                     # display wsl version
wsl --status                      # show wsl status
wsl --unregister Ubuntu-26.04     # unregisters the distro and deletes the root filesystem.

xxx todo more in this section

Build and Test ezApp on Devel PC
================================

Clone ezApp from github:
```
cd ~
git clone https://github.com/sthaid/ezApp.git
```

Set environment variable  xxx improve these comments
xxx don't go into much detail here
- The Android device should be connected to a trusted Wi-Fi network
- The Android Device IP address is displayed in ezApp > Settings
- The 'android-device-ip-addr' can be an IP address, such as 192.168.1.101 or a Hostname (if available)
- The [:port] is only needed if the ezApp Devel_Port had been changed to something other than the default 9000
```
export PATH=$PATH:~/ezApp/bin
export EZAPP_DEVICE=<android-device-ip-addr[:port]>   # example: 192.168.1.101
export EZAPP_PASSWD=<devel-mode-password>             # example: my-secret-password
```

Build ezApp: This will take several minutes, and performs the following steps:
xxx don't go into much detail here
- git clone the SDL repos
- build tools in the bin/src dir
- build a version of ezApp that runs on the Devel PC
- perform a test build of all included miniApps and miniSvcs
```
cd ~/ezApp
make
```

Tests to validate:

* Run a Linux build of ezApp on the Devel PC.
```
    cd ~/ezApp/linux
    make run
    terminate with ctrl-c
```

* Verify ezsh, running on the Devel PC, can connect to the Android ezApp, and execute the 'ls' cmd.
This should list the contents of the /data/data/org.sthaid.ezApp/files/apps directory on the 
Android device.
```
    ezsh ls apps
```

Create a new miniApp
====================

Create a new miniApp, starting with a copy of the Template miniApp.

```
cd ~/ezApp/files/apps
cp -r Template NewApp
cd NewApp
vi template.c    # change "Hello\nWorld" to "NewApp"
```

Test the new miniApp on the Devel PC
====================================

These tests are optional, but very helpful.

Test 1: perform test build of the miniApp using the gcc compiler; if the test
build succeeds, the miniApp is run on the Devel PC using PicoC.
```
cd ~/ezApp/files/apps/NewApp
eztest
```

Test 2: runs ezApp on the Linux Devel PC. This is especially helpful when testing
a miniApp that interacts with a miniSvc. The interactions between miniApp and miniSvc
are not covered by Test 1.
```
cd ~/ezApp/Linux
make run
terminate with ctrl-c
```

Run the new miniApp on Android
==============================

Ensure ezApp is Running on the Android device; and that the ezApp Devel_Mode is ON.

In a new terminal session, on the Devel PC, run ```ezsh logwatch```
to view ezApp debug print messages.

Copy the the new miniApp to the Android Device.
```
cd ~/ezApp/files/apps/NewApp
ezput
```

The NewApp should appear on the Android Device ezApp menu. Tap the '>' to page
through the menu to locate the NewApp.

Tap the NewApp to run it.

APIs available for use by miniApps
==================================

Picoc is extended to support the APIs defined in ~/ezApp/src/ezApp_lib/include:
- sdlx.h: provides miniApp access to SDL features: video, audio, events, and sensors.
- utils.h: various utilities, including json, png, fft, file access, time, location, text-to-speech, ...
- svcs.h: provides miniApp the ability to make a request to a miniSvc
Refer to these files for documentation of the APIs they provide.

These APIs have been added to PicoC via the ezApp/src/picoc/platform/library_unix.c file.

To view the standard C APIs provided by PicoC, inspect files in ezApp/src/picoc/cstdlib.

PicoC Limitations
=================

PicoC is not intended to be a complete implementation of ISO C:
- PicoC supports the essential aspects of the C language.
- For more info on PicoC, refer to ezApp/src/picoc/README.md.

When the PicoC interpreter encounters code that it doesn't understand the error location
is identified, for example:
```
$ cat t1.c
int main() {
    return "hello";
}

$ ./picoc t1.c
    return "hello";
           ^
t1.c:2:10 can't assign int from char*
```

PicoC limitations are described below. It is important to be aware of these
limitations when writing miniApp code.

* Macros must not be defined within a procedure, and macros must return a value.
Examples of supported macros:
```C
    #define RAD2DEG (180. / M_PI)
    #define ACOSD(x)  (acos(x) * RAD2DEG)
```

* The goto statement is implemented but only supports forward gotos, not backward.

* Short-circuit evaluation is not supported. For example the following code
will print "x=0" in standard C, and print "x=1" in PicoC.
```C
    int x = 0;
    if (true || x++) {
        printf("x=%d\n", x);
    }
```

* Pointers to procedures are not supported.

* Stdarg is not supported, '#include <stdarg.h>' will fail.

* Floating point numbers must not start with '.'. For example ```x = .123;``` is not supported.
Instead use ```x = 0.123```.

* Nested ternary operator may give incorrect result. For exmple: ```(true ? 1 : true ? 2 : 3);``` evaluates 
to 2 in PicoC, it should evaluate to 1. Instead use ```(true ? 1 : (true ? 2 : 3))```.

* Static array declarations must include the number of array elements. For example:
```static int x[] = {1,2,3};``` fails. Instead use ```static int x[3] = {1,2,3};```

* Pointer arithmetic issue:
```C
    int x[] = {1,2,3};
    printf("%p\n", x+1);    // this fails to execute in PicoC
    printf("%p\n", &x[1]);  // use this instead
```

* Another pointer arithmetic issue:
```C
    int x[] = {1,2,3};
    // the following prints '1' in standard C; prints 4 in PicoC
    printf("%ld\n", &x[1] - &x[0]);
```

* Initializing an array of struct is not supported. This fails in PicoC:
```C
    struct { 
        int x; 
        int y; 
    } array[3] = { {0,0}, {1,1}, {2,2} };
```

* The base version of picoc runs a C program, when that program terminates,
picoc terminates; the OS then frees all allocations made by the picoc process.
When ezApp calls picoc to run a miniApp and the miniApp terminates, allocations
made by the miniApp (such as malloc, or fopen) are not automatically cleaned up.
MiniApps should be sure to free all memory allocations and close all files
to avoid memory leaks.

miniSvcs
========

The purpose of miniSvcs is to supply information to miniApps about events or state
that occurs while a miniApp is not running. For example, the Steps miniSvc provides
step counts values to the Steps miniApp. These step count values are provided for
all time intervals, as long as the Steps miniSvc is running. Thus, the Steps miniApp
is able to display step count history.

When ezApp starts, the miniSvcs are automatically started:
- a thread is created for each miniSvc
- each miniSvc is run by PicoC in the thread created for it
- if a file named 'stopped' exists in the miniSvc directory, that miniSvc is not started

Services can be Started and Stopped from 'Settings' > 'Services'. When a 
miniSvc has been started, the 'stopped' file is removed. When a miniSvc has been
stopped, the 'stopped' file is created.

A miniSvc that is running continues to run in the background, even when:
- ezApp is stopped; i.e. the user has switched to a different Android app
- the Android device is dozing; note that the miniSvc will run at a reduced rate

The miniSvcs provided with ezApp (Altitude, Location, and Steps), each save
information to a data file. The data file is read, and contents displayed, by a miniApp.

MiniSvcs can also receive and respond to requests from miniApps.
Refer to svcs.h for API details; and view example in the Template miniSvc.

The following APIs can be used by miniSvcc.
- sdlx.h: SENSORS, sdlx_show_toast
- svc.h: svc_wait_for_req, svc_req_completed
- utils.h: all utils.h APIS are okay to use, except:
  - Android text-to-speech
  - Capture device audio
- picoc/cstdlib/*.h

To create a new miniSvc:
- On Devel PC:
```
cd ezApp/files/svcs
cp -r Template NewSvc
cd NewSvc/
ezput
```
- On ezApp: 'Settings' > 'Services'; tap NewSvc to start the new miniSvc.
