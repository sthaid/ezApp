// -----------------  ANDROID  ------------------------------------

#ifdef ANDROID

#include <sdlx.h>
#include <utils.h>
#include <private.h>

#include <SDL3/SDL.h>
#include <jni.h>
#include <unistd.h>

// The following comment is copied from here:
//   https://wiki.libsdl.org/SDL3/SDL_GetAndroidActivity
// Warning (and discussion of implementation details of SDL for Android):
// Local references are automatically deleted if a native function called
// from Java side returns. For SDL this native function is main() itself.
// Therefore references need to be manually deleted because otherwise the
// references will first be cleaned if main() returns (application exit).

// Notes on altitude, from Google AI Overview:
//  "GPS altitude is a height above the WGS84 reference ellipsoid,
//   which is an approximation of the Earth's surface. This value is
//   not the same as height above mean sea level and may require a correction"

// JNI based mehtod signatures:
// References:
//  https://udaniweeraratne.wordpress.com/2016/07/10/how-to-generate-jni-based-method-signature/
//
// goolge search "what are the args to GetMethodID" ...
//   Example of a method signature:
//   - (I)V: A method that takes an int as a parameter and returns void.
//   - (Ljava/lang/String;)I: A method that takes a String object as a parameter and returns an int.
//   - (Ljava/lang/String;I)V: A method that takes a String and an int as parameters and returns void.

// prototype of common routine to call java method
static double call_java1(const char *method_name);
static double call_java2(const char *method_name, char *str);
static double call_java3(const char *method_name, float *array, int num_array_elements);

// android utils init & destroy
void util_android_utils_init(void)
{
    call_java1("android_utils_init");
}

void util_android_utils_destroy(void)
{
    call_java1("android_utils_destroy");
}

// notes:
// - if returned alt_is_wgs84 is false then returned altitude is feet above mean sea leval
// - if returned alt_is_wgs84 is true then returned altitude is feet abve the WGS84 ellepsiod
void util_get_location(double *latitude, double *longitude, double *altitude_ft, bool *alt_is_wgs84) 
{
    int         ms = 0;
    bool        failed, retries_allowed;
    static bool first_call = true;

    // retries are allowd only on the first call
    retries_allowed = first_call;
    first_call = false;

    // loop, allowing retries on the first call
    while (true) {
        // call android java code to get lat/long/alt
        failed = false;
        if (latitude) {
            *latitude = call_java1("get_latitude");
            if (*latitude == INVALID_NUMBER) failed = true;
        }
        if (longitude) {
            *longitude = call_java1("get_longitude");
            if (*longitude == INVALID_NUMBER) failed = true;
        }
        if (altitude_ft) {
            *altitude_ft = call_java1("get_altitude");
            if (*altitude_ft == INVALID_NUMBER) {
                failed = true;
                if (alt_is_wgs84) *alt_is_wgs84 = false;
            } else if (*altitude_ft > (1000000 - 2000)) {
                // notes:
                // - if the altitude is wgs84 the java code adds 1000000 to it,
                //   so that this code knows it is wgs84 altitude
                // - the '- 2000' is to allow for negative wgs84 altitude values
                // - wgs84 altitude is height above a reference ellipsoid, which can be
                //   as much as 350 ft different than mean-sea-level altitude
                *altitude_ft -= 1000000;
                if (alt_is_wgs84) *alt_is_wgs84 = true;
            } else {
                if (alt_is_wgs84) *alt_is_wgs84 = false;
            }
        }

        // if lat/long/alt values have been obtained then return
        if (!failed) {
            return;
        }

        // if retries are not allowed then return
        if (!retries_allowed) {
            return;
        }

        // delay and try again, with 2 sec timeout
        if (ms > 2000) {
            ERROR("timedout\n");
            return;
        }
        usleep(500000);
        ms += 500;
    }
}

// text to speech
void util_text_to_speech(char *text) {
    call_java2("text_to_speech", text);
}
void util_text_to_speech_stop(void) {
    char text[1] = { '\0' };
    call_java2("text_to_speech", text);
}

// foreground service
int util_start_foreground(void) {
    int rc = call_java1("start_foreground");
    return rc == 0 ? 0 : -1;    
}
int util_stop_foreground(void) {
    int rc = call_java1("stop_foreground");
    return rc == 0 ? 0 : -1;    
}
bool util_is_foreground_enabled(void) {
    return call_java1("is_foreground_enabled") == 1;
}

// flashlight
void util_turn_flashlight_on(void) {
    call_java1("turn_flashlight_on");
}
void util_turn_flashlight_off(void) {
    call_java1("turn_flashlight_off");
}
void util_toggle_flashlight(void) {
    call_java1("toggle_flashlight");
}
bool util_is_flashlight_on(void) {
    return call_java1("is_flashlight_on") == 1;
}

// playbackcapture
int util_start_playbackcapture(void) {
    return call_java1("start_playbackcapture");
}
void util_stop_playbackcapture(void) {
    call_java1("stop_playbackcapture");
}
int util_get_playbackcapture_audio(float *array, int num_array_elements) {
    return call_java3("get_playbackcapture_audio", array, num_array_elements);
}

// camera
int util_take_photo(void) 
{
    int rc;
    sdlx_event_t ev;
    char tmp[] = "tmp";
    char photo_jpg[] = "photo.jpg";

    // values copied from _SDLActivity.java
    #define RESULT_OK                         -1
    #define RESULT_CANCELLED                  0
    #define RESULT_FAILED                     1
    #define RESULT_NO_CAMERA                  2
    #define RESULT_FAILED_TO_CREATE_PHOTO_JPG 3;
    #define RESULT_NOT_SET                    99

    // remove existing tmp/photo.jpg file
    util_delete_file(tmp, photo_jpg);

    // take the photo
    rc = call_java1("take_photo");
    if (rc != 0) {
        ERROR("take_photo failed, rc=%d\n", rc);
        return -1;
    }

    // wait for taking the photo to be completed
    while (true) {
        // This call to sdlx_get_event ensures that after taking the
        // photo has completed, and the Android camera code has 
        // finished with the display, the ezApp display becomes visible.
        // Reason why this is needed is not known.
        sdlx_get_event(100000, &ev);

        // check if the taking of the photo has completed
        rc = call_java1("take_photo_complete");

        // if result has been set then break out of loop, 
        // otherwise print that polling continues
        if (rc != RESULT_NOT_SET) {
            break;
        }
        INFO("polling for take_photo_complete\n");
    }

    // if rc is not RESULT_OK then return error
    if (rc != RESULT_OK) {
        ERROR("rc = %d\n", rc);
        return -1;
    }

    // if photo.jpg does not exist then return error
    if (!util_file_exists(tmp, photo_jpg)) {
        ERROR("tmp/photo.jpg does not exist\n");
        return -1;
    }

    // return success
    return 0;
}

// -----------------  COMMON ROUTINES TO CALL JAVA METHOD  -------------------------

// returns:
// - INVALID_NUMBER, when failed, or
// - method specific result value

// call method 'double proc()'
static double call_java1(const char *method_name)
{
    jmethodID method_id = 0;
    double method_ret_double = INVALID_NUMBER;

    // retrieve the JNI environment.,
    // retrieve the Java instance of the SDLActivity,
    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    jclass clazz(env->GetObjectClass(activity));

    // get the method_id, print message if failed
    method_id = env->GetMethodID(clazz, method_name, "()D");

    // if got the method_id then call the method
    if (method_id != 0) {
        method_ret_double = env->CallDoubleMethod(activity, method_id);
    }

    // print error messages
    if (method_id == 0) {
        ERROR("failed to get method_id for %s\n", method_name);
    } else if (method_ret_double == INVALID_NUMBER) {
        ERROR("%s method returned failure\n", method_name);
    }

    // clean up
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);

    // return method result
    return method_ret_double;
}

// call method 'double proc(String s)'
static double call_java2(const char *method_name, char *arg_str)
{
    jmethodID method_id = 0;
    double method_ret_double = INVALID_NUMBER;

    // retrieve the JNI environment.,
    // retrieve the Java instance of the SDLActivity,
    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    jclass clazz(env->GetObjectClass(activity));

    // get the method_id, print message if failed
    method_id = env->GetMethodID(clazz, method_name, "(Ljava/lang/String;)D");

    // if got the method_id then ...
    if (method_id != 0) {
        // Convert C string 'arg_str' to Java String
        // Note - When using JNI's NewStringUTF function, you are creating a new java.lang.String
        //        object within the Java Virtual Machine (JVM). This jstring is a local reference,
        //        and its memory management is handled by the JVM's garbage collector.
        jstring java_string = env->NewStringUTF(arg_str);

        // call method
        method_ret_double = env->CallDoubleMethod(activity, method_id, java_string);
    }

    // print error messages
    if (method_id == 0) {
        ERROR("failed to get method_id for %s\n", method_name);
    } else if (method_ret_double == INVALID_NUMBER) {
        ERROR("%s method returned failure\n", method_name);
    }

    // clean up
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);

    // return method result
    return method_ret_double;
}

// call method 'short[] proc(int arg_unused)' 
double call_java3(const char *method_name, float *caller_array, int num_array_elements)
{
    jmethodID method_id = 0;
    int arg_unused = 0;

    // retrieve the JNI environment.,
    // retrieve the Java instance of the SDLActivity,
    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    jclass clazz(env->GetObjectClass(activity));

    // get the method_id, check for failure
    method_id = env->GetMethodID(clazz, method_name, "(I)[S");
    if (method_id == 0) {
        ERROR("failed to get method_id for %s\n", method_name);
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(clazz);
        return INVALID_NUMBER;
    }

    // call the java method, which will return the array of short elements
    jshortArray array = (jshortArray) env->CallObjectMethod(activity, method_id, arg_unused);
    if (array == nullptr) {
        ERROR("%s method failed\n", method_name);
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(clazz);
        return INVALID_NUMBER;
    }

    // extract array length and elements from the method returned jshortArray
    jsize length = env->GetArrayLength(array);
    jshort* array_elements = env->GetShortArrayElements(array, nullptr);
    if (length != num_array_elements) {
        ERROR("%s method returned unexpected length=%d, expected=%d\n", 
              method_name, length, num_array_elements);
        env->ReleaseShortArrayElements(array, array_elements, JNI_ABORT);
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(clazz);
        return INVALID_NUMBER;
    }

    // return array_elements to caller
    for (int i = 0; i < num_array_elements; i++) {
        caller_array[i] = (float)array_elements[i] / 32767;
    }

    // Release the array elements
    // - JNI_ABORT means changes made to array_elements are not copied back to the Java array.
    // - JNI_COMMIT would copy changes back.
    // - 0 means copy back changes and free the buffer (if a copy was made)
    env->ReleaseShortArrayElements(array, array_elements, JNI_ABORT);

    // clean up
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);

    // return success
    return 0;
}

#else

// -----------------  NOT ANDROID - TEST CODE  ---------------------------

#include <sdlx.h>
#include <utils.h>
#include <private.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

void util_android_utils_init(void) { }

void util_android_utils_destroy(void) { }

void util_get_location(double *latitude, double *longitude, double *altitude, bool *alt_is_wgs84)
{
    #define HOME_LATITUDE     42.4222
    #define HOME_LONGITUDE   -71.6226
    #define HOME_ALTITUDE_FT  454.0

    if (latitude) {
        *latitude = HOME_LATITUDE;
    }
    if (longitude) {
#if 1
        *longitude = HOME_LONGITUDE;
#else
        static time_t tstart;

        // simulate velocity in west direction, for testing
        if (tstart == 0) {
            tstart = time(NULL);
        }

        #define RATE 600.0  // mph
        #define COS_LAT 0.738
        *longitude = HOME_LONGITUDE - 
                     (RATE * (time(NULL) - tstart) / 3600.) / 
                     (COS_LAT * 69.) ;
#endif
    }
    if (altitude) {
        *altitude = HOME_ALTITUDE_FT;
    }
    if (alt_is_wgs84) {
        *alt_is_wgs84 = false;
    }
}

void util_text_to_speech(char *text) { }
void util_text_to_speech_stop(void) { }

int util_start_foreground(void) { return -1; }
int util_stop_foreground(void) { return -1; }
bool util_is_foreground_enabled(void) { return false; }

void util_turn_flashlight_on(void) { }
void util_turn_flashlight_off(void) { }
void util_toggle_flashlight(void) { }
bool util_is_flashlight_on(void) { return false; }

int util_start_playbackcapture(void) { ERROR("this routine only supported on Android\n"); return -1; }
void util_stop_playbackcapture(void) { }
int util_get_playbackcapture_audio(float *array, int num_array_elements) { return INVALID_NUMBER; }

static void remove_trailing_newline(char *s)
{
    int len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }
}

// xxx comment
int util_take_photo(void)
{
    static bool first_call = true;
    static char *jpg_files[10]; // xxx define
    static int max_jpg_files;
    static int idx;

    char *file;
    char  cmd[200];
    int   rc;

    // on first call make list of test jpg files that are in dir $HOME/ezApp_test_photos
    if (first_call) {
        FILE *fp;
        char s[200];

        first_call = false;

        fp = popen(" find $HOME/ezApp_test_photos/ -type f -name \"*.jpg\"", "r");
        while (fgets(s, sizeof(s), fp) != NULL) {
            remove_trailing_newline(s);
            jpg_files[max_jpg_files++] = strdup(s);
        }
        pclose(fp);

        //for (int i = 0; i < max_jpg_files; i++) {
        //    INFO("jpg test file: %s\n", jpg_files[i]);
        //}
    }

    // return error if there are no jpg test files found
    if (max_jpg_files == 0) {
        ERROR("no jpg test files\n");
        return -1;
    }

    // copy one of the jpg test files to tmp/photos.jpg;
    // advance idx so that the next call will copy a different jpg test file
    file = jpg_files[idx++ % max_jpg_files];
    INFO("file %s\n", file);
    sprintf(cmd, "cp %s tmp/photo.jpg", file);
    rc = system(cmd);
    rc = WEXITSTATUS(rc);
    if (rc != 0) {
        return -1;
    }

    // success
    return 0;
}

#endif
