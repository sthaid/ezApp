#ifndef __STEPS_H__
#define __STEPS_H__

#define YEAR0     2026
#define MAX_YEAR  20

#define STEPS_FILENAME "steps.dat"
#define STEPS_FILE_VERSION 0x55aa55aa00000002

typedef struct {
    unsigned long version;

    unsigned int year[MAX_YEAR];
    unsigned int month[MAX_YEAR][12];
    unsigned int day[MAX_YEAR][12][31];
    unsigned int hour[MAX_YEAR][12][31][24];
} steps_file_t;

#endif
