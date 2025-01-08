#ifndef GUI_H
#define GUI_H

#include "media.h"
#include "raylib.h"

#define MAX_MEDIA 10
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

typedef enum {
    GUI_STATE_MARKER_START,
    GUI_STATE_MARKER_END,
} GuiStateMarker;

typedef struct {
    int is_playing;
    int end_of_file;

    // For clipping purposes.
    int64_t start_timestamp, end_timestamp;

    Media *media;

    Texture2D texture;
    AudioStream audio;
} MediaStateWrapper;


// Layout definition
typedef struct {
    Rectangle videoAreaBorder, videoArea, videoProgressArea, playButton,
        resetButton, exportButton, mediaArea;
} GuiLayout;

typedef struct {
    int media_count;
    int current_media_idx;
    Media *current_media;

    // This is used to resize the video data to display on the screen.
    int video_area_width, video_area_height, video_destination_fmt;

    double now, elapsed;
    int target_fps;

    GuiLayout layout;
    MediaStateWrapper *medias[MAX_MEDIA];
} GuiState;

void init_layout(GuiLayout *layout);

// Media state related functions
MediaStateWrapper *media_state_wrapper_alloc();

int media_state_wrapper_init(MediaStateWrapper *media_state, int dst_frame_w,
                             int dst_frame_h, enum AVPixelFormat dst_frame_fmt, const char *filename);
void media_state_wrapper_free(MediaStateWrapper *media_state);

// Gui state related functions
GuiState *gui_state_alloc();

void gui_state_init(GuiState *state);

void gui_state_load_media(GuiState *state, const char *filename);
int gui_state_remove_media(GuiState *state);
void gui_state_play_media(GuiState *state);
void gui_state_reset_media(GuiState *state);
void gui_state_add_marker(GuiState *state, GuiStateMarker marker_type);

void gui_state_media_down(GuiState *state);
void gui_state_media_up(GuiState *state);

int gui_state_update(GuiState *state);
void gui_state_draw(GuiState *state);

void gui_state_run(GuiState *state);

void gui_state_free(GuiState *state);

#endif  // GUI_H