// One way to test the accuracey of the sunrise/sunset times calculated,
// is use Google search: "what are sunrise and sunset times in your-town"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
    
#include <sdlx.h>
#include <utils.h>

#include "apps/Clock/common.h"

#define JD2000 2451545.0

#define SIND(x)   (sin((x)*DEG2RAD))
#define COSD(x)   (cos((x)*DEG2RAD))
#define TAND(x)   (tan((x)*DEG2RAD))
#define ACOSD(x)  (acos(x)*RAD2DEG)
#define ASIND(x)  (asin(x)*RAD2DEG)

#define RAD2DEG (180. / M_PI)
#define DEG2RAD (M_PI / 180.)

static void make_local_time_str(double jd, char *str, char *debug);
static void jd2ymdh(double jd, int *year, int *month, int *day, double *hour);
static double ymdh2jd(int yr, int mn, int day, double hour);
static void hr2hms(double hr, int * hour, int * minute, int * seconds);

// https://en.wikipedia.org/wiki/Sunrise_equation#Hour_angle
void sunrise_sunset_calc(char *sunrise, char *sunset, char *midday, char *daytime)
{
    int              n, year, month, day;
    double           jd, jstar, M, C, lambda, jtransit, declination, hour_angle, jset, jrise, jmid;
    double           latitude, longitude;
    time_t           t;
    struct           tm tm;

    // preset return string values
    strcpy(sunrise, "N/A");
    strcpy(sunset, "N/A");
    strcpy(midday, "N/A");
    strcpy(daytime, "N/A");

    // get location
    util_get_location(&latitude, &longitude, NULL, NULL);
    if (latitude == INVALID_NUMBER || longitude == INVALID_NUMBER) {
        printf("E %s: failed to get location\n", progname);
        return;
    }

    // get the current utc year, month, and day
    t = time(NULL);
    gmtime_r(&t, &tm);
    year = tm.tm_year + 1900;
    month = tm.tm_mon + 1;
    day = tm.tm_mday;

    // get julian date
    jd = ymdh2jd(year, month, day, 0);

    // calculate number of julian days since JD2000 epoch
    n = ceil(jd - JD2000 + 0.0008);

    // mean solar noon
    jstar = n - longitude / 360;

    // solar mean anomaly
    M = (357.5291 + 0.98560028 * jstar);
    if (M < 0) {
        printf("E %s: BUG M < 0\n", progname);
        return;
    }
    while (M >= 360) M -= 360; 

    // equation of the center
    C = 1.9148 * SIND(M) + 0.0200 * SIND(2*M) + 0.0003 * SIND(3*M);

    // ecliptic longitude
    lambda = (M +  C + 180 + 102.9372);
    if (lambda < 0) {
        printf("E %s: BUG lambda < 0\n", progname);
        return;
    }
    while (lambda >= 360) lambda -= 360;

    // solar transit
    jtransit = JD2000 + jstar + 0.0053 * SIND(M) - 0.0069 * SIND(2*lambda);

    // declination of the sun
    declination = ASIND(SIND(lambda) * SIND(23.44));

#if 0
    // hour angle for sun center and no refraction correction
    hour_angle = ACOSD(-TAND(latitude) * TAND(declination));
#else
    // hour angle with correction for refraction and disc diameter
    hour_angle = ACOSD( (SIND(-0.83) - SIND(latitude) * SIND(declination)) /
                       (COSD(latitude) * COSD(declination)));
#endif

    // calculate sunrise and sunset julian date
    jset = jtransit + hour_angle / 360;
    jrise = jtransit - hour_angle / 360;
    jmid = (jset + jrise) / 2;

    // convert julian date to the local time strings that are returned
    make_local_time_str(jrise, sunrise, "calc RISE");
    make_local_time_str(jset, sunset,   "calc SET ");
    make_local_time_str(jmid, midday,   "calc MID ");

    // determine the daytime length
    int minutes, hours;
    minutes = nearbyint((jset - jrise) * 1440);
    hours = minutes / 60;
    minutes -= 60 * hours;
    sprintf(daytime, "%02d:%02d", hours, minutes);
}

static void make_local_time_str(double jd, char *str, char *debug)
{
    int       year, month, day;
    int       hour, minute, seconds;
    double    hr;
    struct tm tm_gmt, tm_local;
    time_t    t;

    jd2ymdh(jd, &year, &month, &day, &hr);
    hr2hms(hr, &hour, &minute, &seconds);

    memset(&tm_gmt,0,sizeof(tm_gmt));
    tm_gmt.tm_sec   = seconds;
    tm_gmt.tm_min   = minute;
    tm_gmt.tm_hour  = hour;
    tm_gmt.tm_mday  = day;
    tm_gmt.tm_mon   = month - 1;     // 0 to 11
    tm_gmt.tm_year  = year - 1900;   // based 1900
    t = timegm(&tm_gmt);

    localtime_r(&t, &tm_local);
    printf("I %s: %s %02d/%02d/%d %02d:%02d:%02d\n", 
           progname, debug,
           tm_local.tm_mon+1, tm_local.tm_mday, tm_local.tm_year+1900,
           tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec);

    sprintf(str, "%02d:%02d", tm_local.tm_hour, tm_local.tm_min);
}

// based on https://aa.usno.navy.mil/faq/docs/JD_Formula.php
//
// COMPUTES THE GREGORIAN CALENDAR DATE (YEAR,MONTH,DAY)
// GIVEN THE JULIAN DATE (JD).
static void jd2ymdh(double jd, int *year, int *month, int *day, double *hour)
{
    int JD = jd + 0.5;
    int I,J,K,L,N;

    L= JD+68569;
    N= 4*L/146097;
    L= L-(146097*N+3)/4;
    I= 4000*(L+1)/1461001;
    L= L-1461*I/4+31;
    J= 80*L/2447;
    K= L-2447*J/80;
    L= J/11;
    J= J+2-12*L;
    I= 100*(N-49)+I+L;

    *year= I;
    *month= J;
    *day = K;

    double jd0 = ymdh2jd(*year, *month, *day, 0);
    *hour = (jd - jd0) * 24;
}

// https://idlastro.gsfc.nasa.gov/ftp/pro/astro/jdcnv.pro
// Converts Gregorian dates to Julian days
static double ymdh2jd(int yr, int mn, int day, double hour)
{
    int L, julian;

    L = (mn-14)/12;    // In leap years, -1 for Jan, Feb, else 0
    julian = day - 32075 + 1461*(yr+4800+L)/4 +
             367*(mn - 2-L*12)/12 - 3*((yr+4900+L)/100)/4;

    return (double)julian + (hour/24.) - 0.5;
}

static void hr2hms(double hr, int * hour, int * minute, int * seconds)
{       
    double secs = hr * 3600;
        
    *hour = secs / 3600;
    secs -= 3600 * *hour;
    *minute = secs / 60;
    secs -= *minute * 60;
    *seconds = nearbyint(secs);
}  
