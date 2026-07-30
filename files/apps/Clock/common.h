#ifndef __COMMON_H__
#define __COMMON_H__

char *progname;
char *data_dir;

void sunrise_sunset_calc(char *sunrise, char *sunset, char *midday, char *daytime);
void sunrise_sunset_web(char *sunrise, char *sunset, char *midday, char *daytime);

#endif
