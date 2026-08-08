ezApp Overview
==============

EzApp is intended to run on Android Smartphones.

EzApp runs miniApps and miniSvcs that are written in the C Language. These
miniApps and miniSvcs are executed by a C language interpreter.

Users can develop their own miniApps and miniSvcs. To develop miniApps and 
miniSvcs a Development PC is required. Refer to miniApps.md for how to build miniApps
and miniSvcs.

The Simple DirectMedia Layer (SDL) provides the framework from which the
Android ezApp APK is built. SDL provides an Android Java Shim, which calls
the C language ezApp entry point, SDL_main, found in src/ezApp/main.c.

SDL provides support for: Graphics, Input Handling, Audio, Events, and more.

The PicoC C language interpreter is used to execute the miniApps. PicoC includes
support for some standard C language header files, such as stdio.h, string.h, etc.

Additional header files have been  added to PicoC to provide APIs needed for miniApps.
The following header files are provided in picoc by code in src/picoc/platform/library_unix.c.
- sdlx.h:  provides core functions that are required by miniApps. These functions
           rely on SDL, and are named sdlx_<name>.
- utils.h: miscellaneous utilities, including: file access, json, png, fft, location, 
           text to speech, and playback capture.
- svcs.h:  provides ability for a miniApp to make a request to a miniSvc.

The sdlx.h, utils.h, and svcs.h files are also provided in src/ezApp_lib/include.
API documentation is included in these header files.

Directory structure:
- files/apps: miniApps Altitude, Calc, Clock, ...
- files/svcs: miniSvcs Altitude, Location, Steps, ...
- linux: build & run ezApp on Linux devel PC
- src:        
  - ezApp: C language main entry point, called from the SDL Java shim
  - ezApp_lib: code for the API routines defined in sdlx.h, utils.h and svcs.h
  - SDL, SDL_mixer, SDL_ttf: populated with SDL code, from github,  by top level Makefile
  - cJSON, kissfft, libmp3lame, lodepng, picoc: copies of code from github repos
  - openssl: populated and built when android Makefile is run to build the Android APK
- bin: contains scripts used to develop/test ezApp and miniApps
- android: the Android Package Kit (APK) is built and installed using Makefile in this directory
- doc: contains documentation

ezApp Settings
==============

Select ezApp 'Settings':
- Readme, Licenses, Credits: view these files
- Services: stop or start miniSvcs
- Rec_Gain: adjust record gain
- Rec_Silence: adjust record volume silence level
- RecTest: make a test recording and playback
- Foreground: allow ezApp miniSvcs to continue running when the device is dozing
- Event_box: display box around available selections
- Devel_Mode: enable developer mode
- Devel_Port: set devel mode port, usually no need to modify the default value
- Devel_Password: set devel mode password, minimum length 4 chars
- Reset_Apps_And_svcs: reset apps and svcs to their original contents,
  all files that have been created will be lost
