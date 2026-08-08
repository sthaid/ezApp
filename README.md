
UNDER CONSTRUCTION
==================

ezApp
=====

EzApp runs on Android Smartphones.

EzApp runs miniApps and miniSvcs that
are written in the C Language. These
miniApps and miniSvcs are executed by
a C language interpreter.

Users can develop their own miniApps
and miniSvcs. To develop miniApps and
miniSvcs a PC is required.

When ezApp is first run on your Android
device, the following permissions will
be requested:

- Post Notifications
- Access Coarse Location
- Access Fine Location
- Activity Recognition
- Record Audio

If some of these permissions are not
granted then some ezApp capabilities
will not function. For example, if
'Activity Recognition' is not granted
the Step counter will not function.

Source code is here:
https://github.com/sthaid/ezApp.git.
See the doc directory for details.

miniApps
========

The following miniApps are included,
along with their C language source code.
Each miniApp contains a README which
can be viewed by tapping the '?'.

- Altitude: View current altitude, and
            history.
- Calc:     Hex / Decimal calculator,
            32 or 64 bit selectable.
- Clock:    Analog clock, also displays
            sunrise and sunset times.
- ColrOrgn: Record or play audio, and
            display color organ.
- Compass:  View magnetic or true
            heading.
- FlshLite: Toggles device flashlight.
- Light:    Sets screen to red or white.
- Location: View current location and
            location history.
- Log:      View  message from ezApp
             and miniApp printf.
- Memo:     Record an audio memo.
- Morse:    Practice morse code.
- Paddle:   Ball and paddle game.
- Piano:    Beginner Piano simulator.
            Includes several melodies.
- Reversi:  Play Reversi.
- Steps:    View Steps and Miles for
            specified day, month, or
            year.
- Template: 'Hello World' example.
- Test:     Unit Test.
- Tilt:     Level, supports horizontal
            and vertical orientations,
            and calibration.
- Weather   View and speaks the weather
            forecast, from weather.gov.

miniSvcs
========

MiniSvcs run in the background while
ezApp is active. The MiniSvcs continue
to run when ezApp is backgrounded, or
the Android is in Doze Mode.

The following MiniSvcs are included.
These provide support for miniApps
by saving data in files, or responding
to the svc_make_req call issued by a
miniApp.

- Altitude: Saves altitude history.
- Location: Saves location history.
- Steps:    Saves step count history.
- Template: A miniSvc example.

MiniSvcs can be stopped or started by
selecting Settings > Services.

