#include <std_hdrs.h>

#include <sdlx.h>
#include <utils.h>
#include <private.h>

#include <SDL3/SDL.h>

//
// defines
//

#define MAX_SENSOR_INFO 256
#define MAX_SENSOR_ID   256

#define TEN_MS 10000

// these sensor type defines are a subset of the sensor types; 
// full list is in the NDK:
// ~/android/sdk/ndk/27.0.12077973/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/android/sensor.h
#define ASENSOR_TYPE_ACCELEROMETER       1    // device + gravity accel
#define ASENSOR_TYPE_MAGNETIC_FIELD      2
#define ASENSOR_TYPE_GYROSCOPE           4
#define ASENSOR_TYPE_LIGHT               5
#define ASENSOR_TYPE_PRESSURE            6
#define ASENSOR_TYPE_PROXIMITY           8
#define ASENSOR_TYPE_GRAVITY             9    // gravity accel only
#define ASENSOR_TYPE_LINEAR_ACCELERATION 10   // device accel only
#define ASENSOR_TYPE_ROTATION_VECTOR     11
#define ASENSOR_TYPE_RELATIVE_HUMIDITY   12   // not supported on most smartphone devices
#define ASENSOR_TYPE_AMBIENT_TEMPERATURE 13   // not supported on most smartphone devices
#define ASENSOR_TYPE_SIGNIFICANT_MOTION  17
#define ASENSOR_TYPE_STEP_DETECTOR       18
#define ASENSOR_TYPE_STEP_COUNTER        19

//
// typedefs
//

//
// variables
//

static sdlx_sensor_info_t sensor_info_tbl[MAX_SENSOR_INFO];
static int               max_sensor_info_tbl;

static SDL_Sensor       *sensor[MAX_SENSOR_ID];  // indexed by id

static int               id_step_counter;
static int               id_gravity_accel;
static int               id_device_accel;
static int               id_magnetic_field;
static int               id_pressure;

//
// prototypes
//

// -----------------  INIT -------------------------------

int sdlx_sensor_init(void)
{
    int            i, max, num_sensors;
    SDL_SensorID  *ids;

    INFO("initializing\n");

    // initialize SDL sensor
    if (!SDL_InitSubSystem(SDL_INIT_SENSOR)) {
        ERROR("SDL_Init SENSOR failed, %s\n", SDL_GetError());
        return -1;
    }

    // get list of sensor ids
    ids = SDL_GetSensors(&num_sensors);
    if (ids == NULL) {
        ERROR("SDL_GetSensors returned NULL\n");
        return -1;
    }
    INFO("num_sensors =%d\n", num_sensors);

    // loop over returned list of sensor ids, and save info in sensor_info_tbl
    max = 0;
    for (i = 0; i < num_sensors; i++) {
        // check if sensor is device private
        if (SDL_GetSensorNonPortableTypeForID(ids[i]) >= 65536) {
            continue;
        }

        // save sensor id, type, non-portable-type, and name in sensor_info_tbl
        sensor_info_tbl[max].id    = ids[i];
        sensor_info_tbl[max].type  = SDL_GetSensorNonPortableTypeForID(ids[i]);
        sensor_info_tbl[max].name  = (char*)SDL_GetSensorNameForID(ids[i]);
        max++;
    }
    max_sensor_info_tbl = max;

    // print the info from sensor_info_tbl
    for (i = 0; i < max_sensor_info_tbl; i++) {
        INFO("%2d %2d %s\n",
             sensor_info_tbl[i].id, 
             sensor_info_tbl[i].type, 
             sensor_info_tbl[i].name);
    }

    // free the list of ids
    SDL_free(ids);

    // find sensors by searching the sensor_info_tbl
    id_step_counter   = sdlx_sensor_find(ASENSOR_TYPE_STEP_COUNTER);
    id_gravity_accel  = sdlx_sensor_find(ASENSOR_TYPE_GRAVITY);
    id_device_accel   = sdlx_sensor_find(ASENSOR_TYPE_LINEAR_ACCELERATION);
    id_magnetic_field = sdlx_sensor_find(ASENSOR_TYPE_MAGNETIC_FIELD);
    id_pressure       = sdlx_sensor_find(ASENSOR_TYPE_PRESSURE);

    // return success
    INFO("success\n");
    return 0;
}

void sdlx_sensor_quit(void)
{
    INFO("quitting\n");

    // quit SDL sensor
    SDL_QuitSubSystem(SDL_INIT_SENSOR);
}

// -----------  APIS AVAILABLE IN PICOC  --------------

sdlx_sensor_info_t *sdlx_sensor_get_info_tbl(int *max)
{
    *max = max_sensor_info_tbl;
    return sensor_info_tbl;
}

// returns sensor id, or -1 if no sensor found for 'type'
int sdlx_sensor_find(int type)
{
    int i;

    // search sensor_info_tbl for 'type' requested by caller
    for (i = 0; i < max_sensor_info_tbl; i++) {
        if (type == sensor_info_tbl[i].type) {
            break;
        }
    }
    if (i == max_sensor_info_tbl) {
        return -1;
    }

    // return id
    return sensor_info_tbl[i].id;
}

int sdlx_sensor_read_raw(int id, float *data, int num_values)
{
    bool  succ;

    // preset return data to 0
    memset(data, 0, num_values * sizeof(float));

    // validate args
    if (id < 0) {
        return -1;
    } else if (id >= MAX_SENSOR_ID) {
        ERROR("id %d is out of range\n", id);
        return -1;  
    }
    if (num_values < 1 || num_values > 16) {
        ERROR("num_values %d is out of range\n", num_values);
        return -1;
    }

    // if not already open then call SDL_OpenSensor
    if (sensor[id] == NULL) {
        sensor[id] = SDL_OpenSensor(id);
        if (sensor[id] == NULL) {
            ERROR("failed to open sensor id %d, %s\n", id, SDL_GetError());
            return -1;
        }
    }

    // get the sensor data
    succ = SDL_GetSensorData(sensor[id], data, num_values);
    if (!succ) {
        ERROR("SDL_GetSensorData failed for id %d, %s\n", id, SDL_GetError());
        return -1;
    }

    // success
    return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - 

#define RAD_TO_DEG (180 / M_PI)
#define DEG_TO_RAD (M_PI / 180)

int sdlx_sensor_read_step_counter(unsigned long *step_count_arg)
{
    unsigned long step_count;
    int           rc;

    // NOTES: 
    // - the step_counter sensor is a special case, returning a 64 bit integer;
    //   refer to NDK ASensorEvent, which is included in the comment section
    //   at the end of this file
    // - first read of the step counter is observed to return an incorrect value of 0

    // read step counter sensor
    rc = sdlx_sensor_read_raw(id_step_counter, (float*)&step_count, 2);
    if (rc != 0) {
        *step_count_arg = INVALID_NUMBER;
        return -1;
    }

    // if step_count value is 0 then return error
    if (step_count == 0) {
        *step_count_arg = INVALID_NUMBER;
        return -1;
    }

    // return step count sensor value
    *step_count_arg = step_count;
    return 0;
}

// This routine reads the acceleration of the device with the 
// acceleration due to gravity removed. Units m/s^2.
// - x-axis: left to right
// - y-axis: bottom to top
// - z-axis: perpendicular to the screen pointing to user
int sdlx_sensor_read_device_accel(double *ax, double *ay, double *az)
{
    float data[3];
    int   rc;

    // read device acceleration sensor data
    rc = sdlx_sensor_read_raw(id_device_accel, data, 3);
    if (rc != 0) {
        *ax = *ay = *az = INVALID_NUMBER;
        return -1;
    }

    // return device accelerometer values
    *ax = data[0];
    *ay = data[1];
    *az = data[2];
    return 0;
}

// Reads the gravity acceleration sensor. Unit m/s^2.
// - x-axis: left to right
// - y-axis: bottom to top
// - z-axis: perpendicular to the screen pointing to user
int sdlx_sensor_read_gravity_accel(double *ax, double *ay, double *az)
{
    float data[3];
    int   rc;

    // read gravity acceleration sensor data
    rc = sdlx_sensor_read_raw(id_gravity_accel, data, 3);
    if (rc != 0) {
        *ax = *ay = *az = INVALID_NUMBER;
        return -1;
    }

    // return gravity accelerometer values
    *ax = data[0];
    *ay = data[1];
    *az = data[2];
    return 0;
}

int sdlx_sensor_read_roll_pitch(double *roll, double *pitch)
{
    float  data[3];
    double ax, ay, az;
    int    rc;

    // read gravity acceleration data
    rc = sdlx_sensor_read_raw(id_gravity_accel, data, 3);
    if (rc != 0) {
        *roll = *pitch = INVALID_NUMBER;
        return -1;
    }

    // convert accelerometer data to roll and pitch; 
    // - positive pitch means top of phone points upward
    // - positive roll means right side of phone is below the left side
    ax = data[0];
    ay = data[1];
    az = data[2];
    *roll  = -atan(ax / sqrt(ay*ay + az*az)) * RAD_TO_DEG;
    *pitch = -atan(-ay / sqrt(ax*ax + az*az)) * RAD_TO_DEG;

    // if nan then set roll/pitch to INVALID_NUMBER, 
    // note: picoc does not support nan
    if (isnan(*roll) || isnan(*pitch)) {
        *roll = INVALID_NUMBER;
        *pitch = INVALID_NUMBER;
        return -1;
    }

    // success
    return 0;
}

int sdlx_sensor_read_mag_heading(double *mag_heading)
{
    float  data[3];
    double mx, my, mz;
    double roll, pitch; 
    double mprimex, mprimey;
    int    rc;

    // read magnetic_heading sensor data
    rc = sdlx_sensor_read_raw(id_magnetic_field, data, 3);
    if (rc != 0) {
        *mag_heading = INVALID_NUMBER;
        return -1;
    }
    my = data[0];
    mx = data[1];
    mz = -data[2];

    // get roll and pitch, which is used to correct the roll/pitch 
    // influence in the calculation of mag_headong
    rc = sdlx_sensor_read_roll_pitch(&roll, &pitch);
    if (rc != 0) {
        *mag_heading = INVALID_NUMBER;
        return -1;
    }
    roll  *= -DEG_TO_RAD;
    pitch *= -DEG_TO_RAD;

    // compute mag_heading, taking into account roll & pitch
    mprimex = mx * cos(pitch) + 
              my * sin(roll) * sin(pitch) - 
              mz * cos(roll) * sin(pitch);
    mprimey = my * cos(roll) + 
              mz * sin(roll);
    *mag_heading = atan2(-mprimey, mprimex) * (180 / M_PI);
    if (isnan(*mag_heading)) {
        *mag_heading = INVALID_NUMBER;
    }

    // adjust mag_heading to range 0-360 degrees,
    // and return success
    if (*mag_heading < 0) {
        *mag_heading += 360;
    }
    return 0;
}

int sdlx_sensor_read_pressure(double *millibars)
{
    float data[3];
    int   rc;

    // read raw sensor data
    rc = sdlx_sensor_read_raw(id_pressure, data, 3);
    if (rc != 0) {
        *millibars = INVALID_NUMBER;
        return -1;
    }

    // return pressure
    *millibars = data[0];
    return 0;
}

#if 0
// -----------------  NOTES  --------------------

------------
REFERENCES 
------------

- Sensors Summary: 
    https://developer.android.com/develop/sensors-and-location/sensors/sensors_overview
    https://developer.android.com/reference/android/hardware/Sensor
    https://developer.android.com/reference/android/hardware/Sensor#TYPE_STEP_COUNTER
- Sensor Events
    https://developer.android.com/reference/android/hardware/SensorEvent.html
    https://developer.android.com/reference/android/hardware/SensorEvent.html#values
- Sensor types
    https://source.android.com/docs/core/interaction/sensors/sensor-types
- SensorType Enum - non portable sensor types
    https://learn.microsoft.com/en-us/dotnet/api/android.hardware.sensortype?view=net-android-35.0
- Android NDK, sensor.h
    ~/android/sdk/ndk/27.0.12077973/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/android/sensor.h
- Permissions
    https://developer.android.com/guide/topics/permissions/overview
    https://developer.android.com/reference/android/Manifest.permission
    SDL_RequestAndroidPermission

---------------
SDL API
---------------

// "In order to use these functions, SDL_Init() must have been called with the SDL_INIT_SENSOR flag.
// This causes SDL to scan the system for sensors, and load appropriate drivers.

// get list of sensors
SDL_GetSensors                     : SDL_SensorID * SDL_GetSensors(int *count);    // returns uint32[]

// get sensor names
SDL_GetSensorNameForID             : const char * SDL_GetSensorNameForID(SDL_SensorID instance_id);
SDL_GetSensorName                  : const char * SDL_GetSensorName(SDL_Sensor *sensor);

// convert SensorID to/from Sensor
SDL_GetSensorID                    : SDL_SensorID SDL_GetSensorID(SDL_Sensor *sensor)
SDL_GetSensorFromID                : SDL_Sensor * SDL_GetSensorFromID(SDL_SensorID instance_id);

// get type of sensor
SDL_GetSensorType                  : SDL_SensorType SDL_GetSensorType(SDL_Sensor *sensor);
SDL_GetSensorTypeForID             : SDL_SensorType SDL_GetSensorTypeForID(SDL_SensorID instance_id);

// open,close,read
SDL_OpenSensor                     : SDL_Sensor * SDL_OpenSensor(SDL_SensorID instance_id);
SDL_CloseSensor                    : void SDL_CloseSensor(SDL_Sensor *sensor);
SDL_GetSensorData                  : bool SDL_GetSensorData(SDL_Sensor *sensor, float *data, int num_values);
SDL_UpdateSensors                  : void SDL_UpdateSensors(void);   # updates the current state of the open sensors

// Android defined sensor types
SDL_GetSensorNonPortableType       : int SDL_GetSensorNonPortableType(SDL_Sensor *sensor);
SDL_GetSensorNonPortableTypeForID  : int SDL_GetSensorNonPortableTypeForID(SDL_SensorID instance_id);

SDL_GetSensorProperties            : SDL_PropertiesID SDL_GetSensorProperties(SDL_Sensor *sensor);    # ret is uint32

-------------
SENSOR TYPES
-------------

https://wiki.libsdl.org/SDL3/SDL_SensorType

typedef enum SDL_SensorType {
    SDL_SENSOR_INVALID = -1,    /**< Returned for an invalid sensor */
    SDL_SENSOR_UNKNOWN,         /**< Unknown sensor type */
    SDL_SENSOR_ACCEL,           /**< Accelerometer */
    SDL_SENSOR_GYRO,            /**< Gyroscope */
    SDL_SENSOR_ACCEL_L,         /**< Accelerometer for left Joy-Con controller and Wii nunchuk */
    SDL_SENSOR_GYRO_L,          /**< Gyroscope for left Joy-Con controller */
    SDL_SENSOR_ACCEL_R,         /**< Accelerometer for right Joy-Con controller */
    SDL_SENSOR_GYRO_R           /**< Gyroscope for right Joy-Con controller */
} SDL_SensorType;

#define SDL_SENSOR_ACCEL    1
#define SDL_SENSOR_GYRO     2
#define SDL_SENSOR_ACCEL_L  3
#define SDL_SENSOR_GYRO_L   4
#define SDL_SENSOR_ACCEL_R  5
#define SDL_SENSOR_GYRO_R   6

--------------------------
SENSOR NON PORTABLE TYPES
--------------------------

~/android/sdk/ndk/27.0.12077973/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/android/sensor.h

#define ASENSOR_TYPE_ACCELEROMETER       1
#define ASENSOR_TYPE_MAGNETIC_FIELD      2
#define ASENSOR_TYPE_GYROSCOPE           4
#define ASENSOR_TYPE_LIGHT               5
#define ASENSOR_TYPE_PRESSURE            6
#define ASENSOR_TYPE_PROXIMITY           8
#define ASENSOR_TYPE_GRAVITY             9
#define ASENSOR_TYPE_LINEAR_ACCELERATION 10
#define ASENSOR_TYPE_ROTATION_VECTOR     11
#define ASENSOR_TYPE_RELATIVE_HUMIDITY   12
#define ASENSOR_TYPE_AMBIENT_TEMPERATURE 13
#define ASENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED 14
#define ASENSOR_TYPE_GAME_ROTATION_VECTOR 15
#define ASENSOR_TYPE_GYROSCOPE_UNCALIBRATED 16
#define ASENSOR_TYPE_SIGNIFICANT_MOTION 17
#define ASENSOR_TYPE_STEP_DETECTOR 18
#define ASENSOR_TYPE_STEP_COUNTER 19
#define ASENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR 20
#define ASENSOR_TYPE_HEART_RATE 21
#define ASENSOR_TYPE_POSE_6DOF 28
#define ASENSOR_TYPE_STATIONARY_DETECT 29
#define ASENSOR_TYPE_MOTION_DETECT 30
#define ASENSOR_TYPE_HEART_BEAT 31
#define ASENSOR_TYPE_DYNAMIC_SENSOR_META 32
#define ASENSOR_TYPE_ADDITIONAL_INFO 33
#define ASENSOR_TYPE_LOW_LATENCY_OFFBODY_DETECT 34
#define ASENSOR_TYPE_ACCELEROMETER_UNCALIBRATED 35
#define ASENSOR_TYPE_HINGE_ANGLE 36
#define ASENSOR_TYPE_HEAD_TRACKER 37
#define ASENSOR_TYPE_ACCELEROMETER_LIMITED_AXES 38
#define ASENSOR_TYPE_GYROSCOPE_LIMITED_AXES 39
#define ASENSOR_TYPE_ACCELEROMETER_LIMITED_AXES_UNCALIBRATED 40
#define ASENSOR_TYPE_GYROSCOPE_LIMITED_AXES_UNCALIBRATED 41
#define ASENSOR_TYPE_HEADING 42

--------------
SENSOR EVENTS
--------------

retrieved by SDL_PollEvent()   eventid = SDL_EVENT_SENSOR_UPDATE

typedef struct SDL_SensorEvent {
    SDL_EventType type; /**< SDL_EVENT_SENSOR_UPDATE */
    Uint32 reserved;
    Uint64 timestamp;   /**< In nanoseconds, populated using SDL_GetTicksNS() */
    SDL_SensorID which; /**< The instance ID of the sensor */
    float data[6];      /**< Up to 6 values from the sensor - 
                             additional values can be queried using SDL_GetSensorData() */
    Uint64 sensor_timestamp; /**< The timestamp of the sensor reading in nanoseconds, 
                                  not necessarily synchronized with the system clock */
} SDL_SensorEvent;

----------
NOTES 
----------

- wakeup sensor - wake the Application Processor from sleep state

-----------------------------
ANDROID SENSORS ON MY DEVICE
-----------------------------

  2: lsm6dso LSM6DSO Accelerometer Non-wakeup                     type=1  nptype=1
  3: AK09918 Magnetometer                                         type=0  nptype=2
  4: lsm6dso LSM6DSO Gyroscope Non-wakeup                         type=2  nptype=4
  5: STK33911 Light  Non-wakeup                                   type=0  nptype=5
  6: lps22hh Pressure Sensor Non-wakeup                           type=0  nptype=6
  7: gravity  Non-wakeup                                          type=0  nptype=9
  8: linear_acceleration                                          type=0  nptype=10
  9: Rotation Vector  Non-wakeup                                  type=0  nptype=11
  10: AK09918 Magnetometer-Uncalibrated                            type=0  nptype=14
  11: Game Rotation Vector  Non-wakeup                             type=0  nptype=15
  12: lsm6dso LSM6DSO Gyroscope-Uncalibrated Non-wakeup            type=0  nptype=16
  13: smd  Wakeup                                                  type=0  nptype=17
  14: step_detector  Non-wakeup                                    type=0  nptype=18
  15: step_counter  Non-wakeup                                     type=0  nptype=19
  16: Tilt Detector  Wakeup                                        type=0  nptype=22
  17: Pick Up Gesture  Wakeup                                      type=0  nptype=25
  18: auto_rotation Screen Orientation Sensor Non-wakeup           type=0  nptype=27
  19: motion_detect                                                type=0  nptype=30
  20: lsm6dso LSM6DSO Accelerometer-Uncalibrated Non-wakeup        type=0  nptype=35
  21: STK33911 Light Strm WideIR Non-wakeup                        type=0  nptype=65578
  22: interrupt_gyro  Non-wakeup                                   type=0  nptype=65579
  23: STK33911 Proximity Strm  Non-wakeup                          type=0  nptype=65582
  24: SensorHub type                                               type=0  nptype=65586
  25: STK33911 Light Strm  Non-wakeup                              type=0  nptype=65587
  26: Wake Up Motion  Wakeup                                       type=0  nptype=65590
  27: STK33911 Proximity  Wakeup                                   type=0  nptype=65592
  28: call_gesture  Wakeup                                         type=0  nptype=65594
  29: STK33911 Auto Brightness Light  Non-wakeup                   type=0  nptype=65601
  30: Pocket mode  Wakeup                                          type=0  nptype=65605
  31: Led Cover Event  Wakeup                                      type=0  nptype=65606
  32: Light seamless  Wakeup                                       type=0  nptype=65614
  33: Flip Cover Detector  Wakeup                                  type=0  nptype=65639
  34: Sar BackOff Motion  Wakeup                                   type=0  nptype=65643
  35: Drop Classifier  Wakeup                                      type=0  nptype=65644
  36: Seq Step  Wakeup                                             type=0  nptype=65647
  37: Pocket Position Mode  Wakeup                                 type=0  nptype=65698
  38: TSL2585 Rear ALS                                             type=0  nptype=65577
  39: Touch Proximity Sensor                                       type=0  nptype=65596
  40: Hall IC                                                      type=0  nptype=65600
  41: Palm Proximity Sensor version 2                              type=0  nptype=8
  42: Motion Sensor                                                type=0  nptype=65559
  43: Orientation Sensor                                           type=0  nptype=3

--------------------------------------
ANDROID SENSOR EVENT STRUCT, FROM NDK
--------------------------------------

/home/haid/android/sdk/ndk/27.0.12077973/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/android

/**
 * Information that describes a sensor event, refer to
 *  https://developer.android.com/reference/android/hardware/SensorEvent
 */
typedef struct ASensorEvent {
    /* sizeof(struct ASensorEvent) */
    int32_t version;
    /** The sensor that generates this event */
    int32_t sensor;
    /** Sensor type for the event, such as {@link ASENSOR_TYPE_ACCELEROMETER} */
    int32_t type;
    /** do not use */
    int32_t reserved0;
    /**
     * The time in nanoseconds at which the event happened, and its behavior
     * is identical to <a href="/reference/android/hardware/SensorEvent#timestamp">
     * SensorEvent::timestamp</a> in Java API.
     */
    int64_t timestamp;
    union {
        union {
            float           data[16];
            ASensorVector   vector;
            ASensorVector   acceleration;
            ASensorVector   gyro;
            ASensorVector   magnetic;
            float           temperature;
            float           distance;
            float           light;
            float           pressure;
            float           relative_humidity;
            AUncalibratedEvent uncalibrated_acceleration;
            AUncalibratedEvent uncalibrated_gyro;
            AUncalibratedEvent uncalibrated_magnetic;
            AMetaDataEvent meta_data;
            AHeartRateEvent heart_rate;
            ADynamicSensorEvent dynamic_sensor_meta;
            AAdditionalInfoEvent additional_info;
            AHeadTrackerEvent head_tracker;
            ALimitedAxesImuEvent limited_axes_imu;
            ALimitedAxesImuUncalibratedEvent limited_axes_imu_uncalibrated;
            AHeadingEvent heading;
        };
        union {
            uint64_t        data[8];
            uint64_t        step_counter;
        } u64;
    };

    uint32_t flags;
    int32_t reserved1[3];
} ASensorEvent;

typedef struct ASensorVector {
    union {
        float v[3];
        struct {
            float x;
            float y;
            float z;
        };
        struct {
            float azimuth;
            float pitch;
            float roll;
        };
    };
    int8_t status;
    uint8_t reserved[3];
} ASensorVector;

#endif
