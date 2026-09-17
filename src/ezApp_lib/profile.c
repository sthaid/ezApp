#include <std_hdrs.h>

#include <private.h>
#include <profile.h>

// This file contains Profile stubs, used when building a test version
// of a miniApp, or miniSvc that runs on Linux.
//
// The Picoc implementation of these routines is functional, and the code
// is at ezApp/src/picoc/platform/library_unix.c.

int profile_start(void)
{
    ERROR("not supported in Linux build\n");
    return -1;
}

void profile_stop(int filter_percent)
{
}

