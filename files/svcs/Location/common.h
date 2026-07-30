#ifndef __COMMON_H__
#define __COMMON_H__

// private header file, do not include in client app

// program name and data_dir strings
char *progname;
char *data_dir;

// routines provided in loc_data.c
int read_loc_data(void);
void free_loc_data(void);
void find_closest_loc_data(double latitude, double longitude, char *name, double *miles);
int download_country_loc_data(char *id);

#endif
