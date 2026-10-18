#include <math.h>
#include <string.h>
#include <stdbool.h>

#include "svcs/Location/location.h"

/*
 * Distance from successive GPS fixes.
 *
 * A phone fix (FusedLocationProvider, high accuracy) is typically only
 * good to about 5–10 m, and the error drifts. Adding the straight-line
 * distance between fixes 10 s apart therefore invents a few kilometers
 * per hour while the phone is sitting still, and it inflates a walk by
 * 10–25%. Faster motion is fine: one step is already much larger than
 * the error.
 *
 * This keeps an anchor fix and reports how far the phone has moved from
 * that anchor.
 *
 *   still, walking     report only after the anchor is 25 m behind and
 *                      a later fix is at least another 8 m farther out.
 *                      A noise spike steps out and comes back; it never
 *                      gains that extra 8 m, so it adds nothing. A real
 *                      walk is summed in chunks of about 30 m.
 *   jogging            same idea, with an 8 m gate
 *   bicycle and faster each fix is counted. The step is 50 m or more.
 *
 * A fix that would require more than about 300 m/s (faster than an
 * airliner) is held. If the next fixes come back, it was a glitch and
 * is dropped. If they stay at the new place, or keep going, the move is
 * real and the held distance is included.
 *
 * The value returned for one call is the meters to add for that fix.
 * While walking, several calls return 0 and a later call returns the
 * chunk. The sum is the distance. The first call returns 0.
 *
 * time_sec is seconds from any clock that moves forward. Pass the time
 * of the fix (location.getTime() / 1000.0), not a second clock. A
 * repeated poll of the same fix returns 0. If the previous accepted fix
 * is more than 10 minutes old, this call starts a new track and returns 0:
 * the straight line across a long gap is not the path that was traveled.
 *
 * One track is remembered. Call gps_interval_distance_reset() to drop it.
 */

#define MAX_SPEED_MPS    300.0   /* above airliner cruise: the fix is bad */
#define MAX_GAP_SEC      600.0
#define WALK_BAND_M       25.0   /* still and walking */
#define JOG_BAND_M         8.0
#define PROGRESS_M         8.0   /* must keep going this far past the band */
#define JOG_SPEED_MPS      2.2   /* ~5 mph, a slow jog */
#define FAST_SPEED_MPS     5.0   /* ~11 mph, bicycle and up */
#define SLOW_JUMP_M       50.0   /* a jump this big while slow needs a second look */
#define SLOW_JUMP_MPS      8.0

#define EARTH_RADIUS_M 6371000.0
#define PI             3.14159265358979323846

typedef struct {
    bool   valid;
    double t;
    double lat;                 /* last fix accepted into the track */
    double lon;
    double anchor_lat;          /* last fix included in the sum */
    double anchor_lon;
    double anchor_t;
    double speed;                    /* recent speed, m/s */
    bool   hold;                     /* a suspect fix is waiting */
    int    hold_count;
    double hold_lat;
    double hold_lon;
    double hold_t;
    bool   armed;                    /* band has been crossed; waiting for progress */
    double arm_dist;
} track_t;

static track_t track;

static double larger(double a, double b)
{
    return (a > b) ? a : b;
}

static bool same_fix(double lat1, double lon1, double lat2, double lon2)
{
    return fabs(lat1 - lat2) < 1e-7 && fabs(lon1 - lon2) < 1e-7;
}

/* Meters between two WGS84-ish points. Fine for a walk and for a flight. */
static double meters_between(double lat1, double lon1, double lat2, double lon2)
{
    double p1 = lat1 * (PI / 180.0);
    double p2 = lat2 * (PI / 180.0);
    double dp = (lat2 - lat1) * (PI / 180.0);
    double dl = (lon2 - lon1) * (PI / 180.0);
    double a = sin(dp / 2) * sin(dp / 2) +
               cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
    if (a > 1.0) a = 1.0;
    return 2.0 * EARTH_RADIUS_M * atan2(sqrt(a), sqrt(1.0 - a));
}

/* Gate width for the speed we believe right now. */
static double noise_band(double speed_mps)
{
    if (speed_mps >= FAST_SPEED_MPS) return 0.0;
    if (speed_mps >= JOG_SPEED_MPS)  return JOG_BAND_M;
    return WALK_BAND_M;
}

static void start_track(double lat, double lon, double t)
{
    memset(&track, 0, sizeof track);
    track.valid      = true;
    track.t          = t;
    track.lat        = lat;
    track.lon        = lon;
    track.anchor_lat = lat;
    track.anchor_lon = lon;
    track.anchor_t   = t;
}

void gps_interval_distance_reset(void)
{
    memset(&track, 0, sizeof track);
}

double gps_interval_distance(double latitude, double longitude, double time_sec)
{
    bool   confirmed = false;
    double dt, step, step_speed, motion, band, moved;

    if (!(latitude >= -90.0 && latitude <= 90.0 &&
          longitude >= -180.0 && longitude <= 180.0)) {
        return 0.0;
    }
    if (!track.valid) {
        start_track(latitude, longitude, time_sec);
        return 0.0;
    }

    /* Same coordinates again: the location client was polled, not a new fix. */
    if (track.hold) {
        if (same_fix(latitude, longitude, track.hold_lat, track.hold_lon))
            return 0.0;
    } else if (same_fix(latitude, longitude, track.lat, track.lon)) {
        return 0.0;
    }

    dt = time_sec - track.t;
    if (dt <= 0.0)
        return 0.0;
    if (dt > MAX_GAP_SEC) {
        start_track(latitude, longitude, time_sec);
        return 0.0;
    }

    /*
     * A held fix is a jump we did not believe. The new fix either
     * comes back (the jump was a glitch), stays out there with it
     * (a bad cluster — wait for one more), or moves on (it was real).
     */
    if (track.hold) {
        double from_hold = meters_between(track.lat, track.lon,
                                           track.hold_lat, track.hold_lon);
        double from_here = meters_between(track.lat, track.lon,
                                           latitude, longitude);
        double span = meters_between(track.hold_lat, track.hold_lon,
                                      latitude, longitude);
        double span_dt = time_sec - track.hold_t;
        double span_speed = (span_dt > 0.0) ? span / span_dt : 1e9;

        if (from_here < from_hold * 0.5) {
            track.hold = false;
            track.hold_count = 0;
        } else if (from_hold > 40.0 &&
                   span < larger(25.0, 0.30 * from_hold) &&
                   track.hold_count < 2) {
            track.hold_count++;
            track.hold_lat = latitude;
            track.hold_lon = longitude;
            track.hold_t = time_sec;
            return 0.0;
        } else if (span_speed <= MAX_SPEED_MPS) {
            track.hold = false;
            track.hold_count = 0;
            confirmed = true;
        } else {
            track.hold_lat = latitude;
            track.hold_lon = longitude;
            track.hold_t = time_sec;
            track.hold_count = 0;
            return 0.0;
        }
    }

    step = meters_between(track.lat, track.lon, latitude, longitude);
    step_speed = step / dt;

    if (!confirmed) {
        bool suspect = false;
        if (step_speed > MAX_SPEED_MPS) {
            suspect = true;
        } else if (track.speed < FAST_SPEED_MPS &&
                   step > SLOW_JUMP_M && step_speed > SLOW_JUMP_MPS) {
            suspect = true;
        } else if (track.speed >= FAST_SPEED_MPS) {
            double allowed = track.speed + larger(8.0, 0.40 * track.speed);
            if (step_speed > allowed && step > 40.0)
                suspect = true;
        }
        if (suspect) {
            track.hold = true;
            track.hold_count = 0;
            track.hold_lat = latitude;
            track.hold_lon = longitude;
            track.hold_t = time_sec;
            return 0.0;
        }
    }

    /*
     * Pick the gate from how fast this step actually was.
     * A stop after driving must use the walking gate immediately,
     * or the driving gate would count the sit-still wander.
     * A real departure is trusted at once so the chunk is not smoothed away.
     */
    motion = track.speed;
    if (confirmed || step_speed > SLOW_JUMP_MPS)
        motion = larger(motion, step_speed);
    else if (step_speed + 1.0 < track.speed)
        motion = step_speed;

    track.t = time_sec;
    track.lat = latitude;
    track.lon = longitude;

    moved = meters_between(track.anchor_lat, track.anchor_lon, latitude, longitude);
    band = noise_band(motion);

    if (moved < band) {
        track.armed = false;
        if (step < 8.0)
            track.speed *= exp(-dt / 20.0);
        return 0.0;
    }

    /* Outside the gate once is not enough; the next fixes must keep going. */
    if (band > 0.0) {
        if (!track.armed) {
            track.armed = true;
            track.arm_dist = moved;
            return 0.0;
        }
        if (moved < track.arm_dist + PROGRESS_M)
            return 0.0;
    }
    track.armed = false;

    {
        double elapsed = time_sec - track.anchor_t;
        double observed;
        if (elapsed < 1.0) elapsed = dt;
        observed = moved / elapsed;
        track.speed = (track.speed < 0.5) ? observed
                                           : (0.5 * track.speed + 0.5 * observed);
    }
    track.anchor_lat = latitude;
    track.anchor_lon = longitude;
    track.anchor_t = time_sec;
    return moved;
}
