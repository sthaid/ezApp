#ifndef __COMMON_H__
#define __COMMON_H__

// defines
#define COW_P 1000   // color organ w/h in portrait orientation
#define COH_P  800  
#define COW_L 1200   // color organ w/h in landscape orientation
#define COH_L 1000

#define DEBUG_FLAG_CYCLE_DUR  1

#define LINE_SPACING (2.00 * sdlx_char_height_dflt)

// variables
char *progname;
char *data_dir;
char  files_dir[100];
int   orientation;
bool  show_controls;
int   debug_flags;

// prototypes of routiens defined in color_organ.c
void color_organ_init(void);
void color_organ_cleanup(void);
void color_organ_display(void);
void color_organ_process_event(sdlx_event_t *ev);
void color_organ_settings(void);

#endif
