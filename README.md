
xxx UNDER CONSTRUCTION xxx           
==========================

ezApp for Android Smartphones
=============================

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
- Camera

If some of these permissions are not
granted then some ezApp capabilities
will not function. For example, if
'Activity Recognition' is not granted
the Step counter will not function.

ezApp source code is here:
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
- Camera:   Take photos. View photos 
            by location or gallery.
- Compass:  View magnetic or true
            heading.
- FlshLite: Toggle device flashlight.
- Location: View current location and
            location history.
- Log:      View messages from ezApp
            and miniApp printf.
- Memo:     Record an audio memo.
- Morse:    Practice morse code.
- Piano:    Beginner Piano simulator.
            Includes several melodies.
- Steps:    View Steps and Miles for
            specified day, month, or
            year.
- Tilt:     Level, supports horizontal
            and vertical orientations,
            and calibration.
- Weather:  Views and speaks the weather
            forecast, from weather.gov.

Games:
- Backgammon: Board game.
- Blackjack   Card game.
- Paddle:     Ball and paddle game.
- Reversi:    Board game.
- TicTacToe:  X and O Grid game

Test and Examples:
- Test:     Unit Test.
- Example1: 'Hello World' example.
- Example2: Sets screen to red or white.

miniSvcs
========

miniSvcs run in the background while
ezApp is active. The miniSvcs continue
to run when ezApp is backgrounded, or
the device is in Doze mode.

The following miniSvcs are included.
These provide support for miniApps
by saving data in files, or responding
to requests from miniApps.

- Altitude: Saves altitude history.
- Location: Saves location history.
- Steps:    Saves step count history.
- Example:  A miniSvc example.

miniSvcs can be stopped or started by
selecting Settings > Services.
