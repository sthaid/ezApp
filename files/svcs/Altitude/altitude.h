#ifndef __ALTITUDE_H__
#define __ALTITUDE_H__

#define YEAR0     2026
#define MAX_YEAR  20

#define ALTITUDE_FILENAME     "altitude.dat"
#define ALTITUDE_FILE_VERSION 0x7777777700000001

// note lowest worldwide altitude is about 1500 ft below sea level

#define NO_ALTITUDE_DATA -2000

typedef struct {
    unsigned long version;
    int altitude_ft[MAX_YEAR][12][31][24];
} altitude_file_t;

#endif
