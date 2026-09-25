#ifndef __PROFILE_H__
#define __PROFILE_H__

#ifdef __cplusplus
extern "C" {
#endif

// Refer to miniApp Test, test.c for a usage example.

// Start profiling. A PicoC thread will be created that will wake 
// at 1 millisecond interval. Each time it wakes, this thread
// will increment a counter for the file and line where the program is
// currently executing.
int profile_start(void);

// Stop profiling, and print results.
// The filter_percent param is used to print only profile counts
// that exceed filter_percent of the max_count.
void profile_stop(int filter_percent);

#ifdef __cplusplus
}
#endif

#endif

