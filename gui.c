#include <libgen.h>
#include "gui.h"

// ##################### LAYOUT FUNCTIONS #####################

void init_layout(GuiLayout *layout) {
    layout->videoAreaBorder =
        (Rectangle){.x = (int)(WINDOW_WIDTH / 3),
                    .y = 10,
                    .width = (int)((2 * WINDOW_WIDTH) / 3) - 20,
                    .height = WINDOW_HEIGHT - 100};

    layout->videoArea = (Rectangle){.x = (int)(WINDOW_WIDTH / 3) + 2,
                                    .y = 12,
                                    .width = (int)((2 * WINDOW_WIDTH) / 3) - 24,
                                    .height = WINDOW_HEIGHT - 104};

    layout->videoProgressArea =
        (Rectangle){.x = layout->videoArea.x,
                    .y = layout->videoArea.y + layout->videoArea.height,
                    .width = layout->videoArea.width,
                    .height = 30};

    layout->playButton =
        (Rectangle){.x = (int)(WINDOW_WIDTH / 3 + WINDOW_WIDTH / 4),
                    .y = layout->videoAreaBorder.height + 30,
                    .width = 100,
                    .height = 35};

    layout->resetButton =
        (Rectangle){.x = (int)(WINDOW_WIDTH / 3 + WINDOW_WIDTH / 4) +
                         layout->playButton.width + 10,
                    .y = layout->videoAreaBorder.height + 30,
                    .width = 60,
                    .height = 35};

    layout->exportButton = (Rectangle){
        .x = (int)(WINDOW_WIDTH / 3 + WINDOW_WIDTH / 4) +
             layout->resetButton.width + layout->playButton.width + 20,
        .y = layout->videoAreaBorder.height + 30,
        .width = 110,
        .height = 35};

    layout->mediaArea = (Rectangle){.x = 10,
                                    .y = 10,
                                    .width = (int)(WINDOW_WIDTH / 3) - 20,
                                    .height = WINDOW_HEIGHT - 20};
}

// ##################### MEDIA STATE FUNCTIONS #####################

MediaStateWrapper *media_state_wrapper_alloc() {
    MediaStateWrapper *media_state =
        (MediaStateWrapper *)malloc(sizeof(MediaStateWrapper));
    if (!media_state) {
        return NULL;
    }

    media_state->is_playing = 0;
    media_state->end_of_file = 0;
    media_state->start_timestamp = 0;
    media_state->end_timestamp = 0;
    media_state->media = NULL;
    media_state->texture = (Texture2D){0};
    media_state->audio = (AudioStream){0};

    return media_state;
}

int media_state_init(MediaStateWrapper *media_state, int dst_frame_w,
                     int dst_frame_h, enum AVPixelFormat dst_frame_fmt,
                     const char *filename) {
    if (!media_state) {
        return -1;
    }

    Media *media = media_alloc();
    if (!media) {
        printf("media_state_init: failed to allocate media\n");
        return -1;
    }

    if (media_init(media, dst_frame_w, dst_frame_h, dst_frame_fmt, filename) <
        0) {
        printf("media_state_init: failed to initialize media\n");
        return -1;
    }

    media_state->is_playing = 0;
    media_state->end_of_file = 0;
    media_state->start_timestamp = 0;
    media_state->end_timestamp = media->fmt_ctx->duration;
    media_state->media = media;

    Image img = GenImageColor(dst_frame_w, dst_frame_h, BLACK);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    AudioStream audio = LoadAudioStream(media->audio_ctx->sample_rate, 32, 2);
    SetAudioStreamVolume(audio, 1.0f);
    PlayAudioStream(audio);

    media_state->texture = tex;
    media_state->audio = audio;

    return 0;
}

// ##################### GUI STATE FUNCTIONS #####################

GuiState *gui_state_alloc() {
    GuiState *state = (GuiState *)malloc(sizeof(GuiState));
    state->media_count = 0;
    state->current_media_idx = 0;

    state->video_area_width = 0;
    state->video_area_height = 0;
    state->video_destination_fmt = AV_PIX_FMT_RGBA;

    state->now = 0;
    state->elapsed = 0;
    state->target_fps = 60;

    state->layout = (GuiLayout){0};

    return state;
}

void gui_state_init(GuiState *state) {
    init_layout(&state->layout);

    state->video_area_width = state->layout.videoArea.width;
    state->video_area_height = state->layout.videoArea.height;
    state->video_destination_fmt = AV_PIX_FMT_RGBA;

    state->now = GetTime();
    state->elapsed = 0;
    state->target_fps = 60;
}

void gui_state_add_media(GuiState *state, MediaStateWrapper *ms) {
    state->medias[state->media_count++] = ms;
    state->current_media_idx = state->media_count - 1;
    state->current_media = state->medias[state->current_media_idx]->media;
}

void gui_state_media_down(GuiState *state) {
    state->current_media_idx =
        (state->current_media_idx + 1) % state->media_count;
    state->current_media = state->medias[state->current_media_idx]->media;
}

void gui_state_media_up(GuiState *state) {
    state->current_media_idx =
        (state->current_media_idx - 1) % state->media_count;
    state->current_media = state->medias[state->current_media_idx]->media;
}

int gui_state_remove_media(GuiState *state) {
    if (state->media_count == 0) {
        printf("gui_state_remove_media: no media to remove\n");
        return -1;
    }

    MediaStateWrapper *ms = state->medias[state->current_media_idx];
    media_free(ms->media);

    UnloadTexture(ms->texture);
    StopAudioStream(ms->audio);
    UnloadAudioStream(ms->audio);

    free(ms);

    // This just shifts the array to the left by one.
    for (int i = state->current_media_idx; i < state->media_count - 1; i++) {
        state->medias[i] = state->medias[i + 1];
    }

    state->media_count--;
    state->current_media_idx =
        state->current_media_idx - 1 < 0 ? 0 : state->current_media_idx - 1;
    state->current_media = state->medias[state->current_media_idx]->media;

    return 0;
}

void gui_state_play_media(GuiState *state) {
    if (state->media_count == 0) {
        printf("gui_state_play_media: no media to play\n");
        return;
    }

    state->medias[state->current_media_idx]->is_playing =
        !state->medias[state->current_media_idx]->is_playing;
    state->now = GetTime();

    if (state->medias[state->current_media_idx]->is_playing) {
        ResumeAudioStream(state->medias[state->current_media_idx]->audio);
    } else {
        PauseAudioStream(state->medias[state->current_media_idx]->audio);
    }
}

void gui_state_reset_media(GuiState *state) {
    if (state->media_count == 0) {
        printf("gui_state_reset_media: no media to reset\n");
        return;
    }

    // TODO: the incr value maybe wont work for longer videos
    if (media_seek(state->medias[state->current_media_idx]->media, -99999,
                   SEEK_BACKWARD) < 0) {
        printf("gui_state_reset_media: failed to seek\n");
        return;
    }

    state->medias[state->current_media_idx]->is_playing = 0;
    state->medias[state->current_media_idx]->end_of_file = 0;
}

int gui_state_update(GuiState *state) {
    if (IsFileDropped()) {
        FilePathList dropped_files = LoadDroppedFiles();
        for (int i = 0; i < (int)dropped_files.count; i++) {
            MediaStateWrapper *media_state = media_state_wrapper_alloc();
            if (!media_state) {
                printf("gui_state_update: failed to allocate media state\n");
                return -1;
            }

            if (media_state_init(media_state, state->video_area_width,
                                 state->video_area_height,
                                 state->video_destination_fmt,
                                 dropped_files.paths[i]) < 0) {
                printf("gui_state_update: failed to initialize media state\n");
                return -1;
            }

            gui_state_add_media(state, media_state);
        }

        UnloadDroppedFiles(dropped_files);
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && state->media_count > 0) {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, state->layout.playButton)) {
            gui_state_play_media(state);
        } else if (CheckCollisionPointRec(mouse, state->layout.resetButton)) {
            gui_state_reset_media(state);
        } else if (CheckCollisionPointRec(mouse, state->layout.exportButton)) {
            // Export video
        }
    }

    if (IsKeyPressed(KEY_DOWN) && state->media_count > 0) {
        gui_state_media_down(state);
    } else if (IsKeyPressed(KEY_UP) && state->media_count > 0) {
        gui_state_media_up(state);
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_X)) {
        gui_state_remove_media(state);
    }

    int ret;
    if (state->media_count > 0 &&
        state->medias[state->current_media_idx]->is_playing) {
        MediaStateWrapper *media_state =
            state->medias[state->current_media_idx];

        if ((ret = media_read_frame(media_state->media)) < 0) {
            if (ret == MEDIA_ERR_EOF) {
                state->medias[state->current_media_idx]->end_of_file = 1;
                state->medias[state->current_media_idx]->is_playing = 0;
                return 0;
            } else {
                TraceLog(LOG_ERROR, "Failed to read frame: %d", ret);
                return -1;
            }
        }

        if ((ret = media_decode(media_state->media)) < 0 &&
            ret != MEDIA_ERR_MORE_DATA) {
            TraceLog(LOG_ERROR, "Failed to decode frame: %d", ret);
            return -1;
        }

        if (fq_empty(media_state->media->queue) && ret == MEDIA_ERR_MORE_DATA) {
            return 0;
        }

        Node *node = fq_dequeue(media_state->media->queue);
        if (!node) {
            TraceLog(LOG_ERROR, "Failed to dequeue frame");
            return -1;
        }

        if (node->type == FRAME_TYPE_VIDEO) {
            UpdateTexture(media_state->texture, node->frame->data[0]);
        } else {
            if (IsAudioStreamProcessed(media_state->audio)) {
                UpdateAudioStream(media_state->audio, node->frame->data[0],
                                  node->frame->nb_samples);
            }
        }
        node_free(node);
    }

    return 0;
}

void gui_state_draw(GuiState *state) {
    DrawRectangleLinesEx(state->layout.mediaArea, 2, GRAY);
    if (state->media_count == 0) {
        DrawText("Drop media files here!", state->layout.mediaArea.x + 15,
                 state->layout.mediaArea.height / 2, 25, GRAY);
    } else {
        for (int i = 0; i < state->media_count; i++) {
            DrawRectangle(state->layout.mediaArea.x, state->layout.mediaArea.y + 35 * i,
                          state->layout.mediaArea.width, 35,
                          i == state->current_media_idx
                              ? SKYBLUE
                              : (i % 2 == 0 ? GRAY : LIGHTGRAY));
            DrawText(basename(state->medias[i]->media->filename),
                     state->layout.mediaArea.x + 10, state->layout.mediaArea.y + 35 * i, 20,
                     i == state->current_media_idx ? BLUE : DARKGRAY);
        }
    }

    DrawRectangleLinesEx(state->layout.videoAreaBorder, 2, GRAY);
    DrawRectangleRec(state->layout.videoArea, WHITE);

    if (state->media_count > 0 &&
        state->medias[state->current_media_idx]->end_of_file) {
        DrawText("End of file", state->layout.videoArea.x + 10, state->layout.videoArea.y + 10, 20,
                 RED);
    } else if (state->media_count > 0) {
        DrawTexture(state->medias[state->current_media_idx]->texture,
                    state->layout.videoArea.x, state->layout.videoArea.y, WHITE);

        // Draw the current position at one tip of the video progress area,
        // and the video duration at the other tip.
        Media *current_media =
            state->medias[state->current_media_idx]->media;

        DrawText(current_media->formatted_position,
                 state->layout.videoProgressArea.x + 10, state->layout.videoProgressArea.y + 5, 20,
                 GRAY);
        DrawText(current_media->formatted_duration,
                 state->layout.videoProgressArea.x + state->layout.videoProgressArea.width - 100,
                 state->layout.videoProgressArea.y + 5, 20, GRAY);
    }

    DrawRectangleRec(state->layout.playButton, LIGHTGRAY);
    DrawText("Play/Pause", state->layout.playButton.x + 5, state->layout.playButton.y + 10, 15, GRAY);

    DrawRectangleRec(state->layout.resetButton, LIGHTGRAY);
    DrawText("Reset", state->layout.resetButton.x + 5, state->layout.resetButton.y + 10, 15, GRAY);

    DrawRectangleRec(state->layout.exportButton, LIGHTGRAY);
    DrawText("Export (.mp4)", state->layout.exportButton.x + 5, state->layout.exportButton.y + 10, 15,
             GRAY);
}

void gui_state_run(GuiState *state) {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Ave");
    InitAudioDevice();

    while (!WindowShouldClose()) {
        PollInputEvents();

        gui_state_update(state);

        BeginDrawing();
        ClearBackground(RAYWHITE);

        gui_state_draw(state);

        EndDrawing();
        SwapScreenBuffer();

        state->elapsed = GetTime() - state->now;
        if (state->media_count > 0 &&
            state->medias[state->current_media_idx]->is_playing) {
            Media *current_media =
                state->medias[state->current_media_idx]->media;

            int stream_index;
            if (current_media->pkt->stream_index ==
                current_media->video_stream_idx) {
                stream_index = current_media->video_stream_idx;
            } else {
                stream_index = current_media->audio_stream_idx;
            }

            double timebase = av_q2d(
                current_media->fmt_ctx->streams[stream_index]->time_base);
            double pts = (double)current_media->pkt->pts;

            double pts_time = timebase * pts;
            double wait_time = pts_time - state->elapsed;

            if (wait_time > 0) {
                WaitTime(wait_time);
            }
        } else {
            if (state->elapsed < 1.0 / state->target_fps) {
                WaitTime(1.0 / state->target_fps - state->elapsed);
            }
        }
    }
}

void gui_state_free(GuiState *state) {
    CloseAudioDevice();
    CloseWindow();

    for (int i = 0; i < state->media_count; i++) {
        media_free(state->medias[i]->media);
        UnloadTexture(state->medias[i]->texture);
        StopAudioStream(state->medias[i]->audio);
        UnloadAudioStream(state->medias[i]->audio);
        free(state->medias[i]);
    }
    free(state);
}