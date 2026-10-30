#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "svcs/Location/location.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define EARTH_RADIUS_METERS 6371000.0

// Kinematic limits
#define MAX_FLYING_SPEED_MPS  300.0   
#define MAX_ACCELERATION_MPS2  15.0   
#define MIN_TIME_DELTA_SEC      0.5     

// Anti-Jitter thresholds
#define MIN_JITTER_DISTANCE_M   2.5    // Distances smaller than 2.5m are treated as sensor noise
#define MIN_MOTION_SPEED_MPS    0.5    // Speeds lower than 0.5 m/s (~1.8 km/h) mean the user is stationary

typedef struct {
    double latitude;   
    double longitude;  
    double timestamp;  
    double velocity;   
    bool is_initialized;
} LocationState;

static LocationState g_last_valid_location;

static double calculate_distance_meters(double lat1, double lon1, double lat2, double lon2) 
{
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double d_lat_rad = (lat2 - lat1) * M_PI / 180.0;
    double d_lon_rad = (lon2 - lon1) * M_PI / 180.0;

    double a = sin(d_lat_rad / 2.0) * sin(d_lat_rad / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(d_lon_rad / 2.0) * sin(d_lon_rad / 2.0);
    
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return EARTH_RADIUS_METERS * c;
}

bool filter_fused_location(double lat, double lon, double timestamp_sec, double *out_distance) 
{
    if (out_distance == NULL) {
        return true; 
    }
    *out_distance = 0.0;

    if (!g_last_valid_location.is_initialized) {
        g_last_valid_location.latitude = lat;
        g_last_valid_location.longitude = lon;
        g_last_valid_location.timestamp = timestamp_sec;
        g_last_valid_location.velocity = 0.0;
        g_last_valid_location.is_initialized = true;
        return false; 
    }

    double dt = timestamp_sec - g_last_valid_location.timestamp;
    if (dt < MIN_TIME_DELTA_SEC) {
        return true; 
    }

    // 1. Calculate raw distance and apparent velocity
    double distance = calculate_distance_meters(g_last_valid_location.latitude, g_last_valid_location.longitude, lat, lon);
    double current_velocity = distance / dt;

    // 2. Absolute Speed Gate
    if (current_velocity > MAX_FLYING_SPEED_MPS) {
        return true;
    }

    // 3. Adaptive Acceleration Gate
    double dv = fabs(current_velocity - g_last_valid_location.velocity);
    double acceleration = dv / dt;

    if (acceleration > MAX_ACCELERATION_MPS2) {
        // If it fails acceleration but the speed is tiny, it's just a large indoor jitter spike.
        // We reject it if it's acting like a moving point, but accept it as stationary if it's small.
        if (current_velocity > 5.0) { 
            return true; 
        }
    }

    // 4. Jitter and Stationary Suppression
    // Check if the movement is too small or too slow to be real intentional travel
    if (distance < MIN_JITTER_DISTANCE_M || current_velocity < MIN_MOTION_SPEED_MPS) {
        // Force outputs to 0 so no phantom distance accumulates
        distance = 0.0;
        current_velocity = 0.0;
        
        // Note: We still update the timestamp and coordinates below. This allows the baseline
        // to stay fresh, preventing a slow, long-term drift from breaking the filter.
    }

    // Update tracking state
    g_last_valid_location.latitude = lat;
    g_last_valid_location.longitude = lon;
    g_last_valid_location.timestamp = timestamp_sec;
    g_last_valid_location.velocity = current_velocity;

    *out_distance = distance;
    return false; 
}

