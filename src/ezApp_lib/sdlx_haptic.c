#include <std_hdrs.h>

#include <sdlx.h>
#include <utils.h>
#include <private.h>

#include <SDL3/SDL.h>

static SDL_Haptic *haptic;

// -----------------  INITIALIZE  ---------------------------------

int sdlx_haptic_init(void)
{
    int           num_haptics;
    SDL_HapticID *haptics_list;
    bool          succ;

    INFO("initializing\n");

    // initialize SDL haptic
    if (!SDL_InitSubSystem(SDL_INIT_HAPTIC)) {
        ERROR("SDL_Init HAPTIC failed, %s\n", SDL_GetError());
        return -1;
    }

    // get list of available haptic devices
    haptics_list = SDL_GetHaptics(&num_haptics);
    if (haptics_list == NULL) {
        ERROR("SDL_GetHaptics failed, %s\n", SDL_GetError());
        return -1;
    }
    if (num_haptics == 0) {
        INFO("num_haptics is 0, returning success\n");
        SDL_free(haptics_list);
        return 0;
    }

    // open the first haptic in the list
    haptic = SDL_OpenHaptic(haptics_list[0]);
    SDL_free(haptics_list);
    if (haptic == NULL) {
        ERROR("SDL_OpenHaptic failed, %s\n", SDL_GetError());
        return -1;
    }

    // init haptic device for simple rumble playback
    succ = SDL_InitHapticRumble(haptic);
    if (!succ) {
        ERROR("SDL_InitHapticRumble failed, %s\n", SDL_GetError());
        SDL_CloseHaptic(haptic);
        haptic = NULL;
        return -1;
    }

    // success
    return 0;
}

void sdlx_haptic_quit(void)
{
    INFO("quitting\n");

    if (haptic) {
        SDL_StopHapticRumble(haptic);
        SDL_CloseHaptic(haptic);
        haptic = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_HAPTIC);
}


// -----------------  VIBRATE  ------------------------------------

// strength arg range is 0 to 1
void sdlx_vibrate(double strength, int duration_ms)
{
    if (haptic) {
        SDL_PlayHapticRumble(haptic, strength, duration_ms);
    }
}
