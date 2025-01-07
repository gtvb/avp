#ifndef GUI_H
#define GUI_H

#include "raylib.h"
#include "media.h"

#define MAX_MEDIA 10

typedef struct {
    int is_playing;
    int end_of_file;

    // For clipping purposes.
    int64_t start_timestamp, end_timestamp;

    Media *media;

    Texture2D texture;
    AudioStream audio;
} MediaStateWrapper;

typedef struct {
    int media_count;
    int current_media_idx;
    Media *current_media;

    // This is used to resize the video data to display on the screen.
    int video_area_width, video_area_height, video_destination_fmt;

    double now, elapsed;
    int target_fps;

    MediaStateWrapper *medias[MAX_MEDIA];
} GuiState;

MediaStateWrapper *media_state_wrapper_alloc();
GuiState *gui_state_alloc();

int gui_state_init(GuiState *state, int video_area_width, int video_area_height,
                   int video_destination_fmt);

int gui_state_add_media(GuiState *state, const char *filename);
int gui_state_remove_media(GuiState *state, int idx);

int gui_state_add_marker(GuiState *state, int media_idx, int64_t timestamp);
int gui_state_remove_marker(GuiState *state, int media_idx, int marker_idx);

int gui_state_playback(GuiState *state);


void gui_state_free(GuiState *state);


#endif // GUI_H