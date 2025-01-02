#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#include "media.h"
#include "raylib.h"

#define MAX_MEDIA 10

typedef struct {
    int is_playing;
    int end_of_file;
    Media *media;
} MediaStateWrapper;

typedef struct {
    int media_count;
    int current_media_idx;

    int video_area_width, video_area_height, video_destination_fmt;

    double now, elapsed;
    int target_fps;

    MediaStateWrapper *medias[MAX_MEDIA];
} AppState;

void app_state_free(AppState *state) {
    for (int i = 0; i < state->media_count; i++) {
        media_free(state->medias[i]->media);
        free(state->medias[i]);
    }
    free(state);
}

// int main(void) {
//     const char *filename = "assets/bigbuckbunny.mp4";
//     int ret;

//     Media *media = media_alloc();
//     if (!media) {
//         printf("Failed to allocate media.\n");
//         return 1;
//     }

//     strcpy(media->filename, filename);

//     if ((ret = media_init(media, 1280, 720, AV_PIX_FMT_RGBA)) < 0) {
//         printf("Failed to initialize media: %d\n", ret);
//         media_free(media);
//         return 1;
//     }

//     // Perform operations with the media
//     // For example, print media information
//     printf("Media filename: %s\n", media->filename);

//     while(1) {
//         if ((ret = media_read_frame(media)) < 0) {
//             if (ret == MEDIA_ERR_EOF) {
//                 printf("End of file\n");
//                 break;
//             } else {
//                 printf("Failed to read frame: %d\n", ret);
//                 break;
//             }
//         }

//         if ((ret = media_decode(media)) < 0) {
//             printf("Failed to decode frame: %d\n", ret);
//             break;
//         }

//         Node *node = fq_dequeue(media->queue);
//         if (!node) {
//             printf("Failed to dequeue frame\n");
//             break;
//         }

//         if (node->type == FRAME_TYPE_VIDEO) {
//             AVFrame *frame = node->frame;
//             printf("Decoded video frame: %dx%d\n", frame->width,
//             frame->height);
//         } else {
//             AVFrame *frame = node->frame;
//             printf("Decoded audio frame: %d samples\n", frame->nb_samples);
//         }
//     }

//     // Free the media
//     media_free(media);
//     return 0;
// }

int main(void) {
    int ret;
    const int window_width = 1280;
    const int window_height = 720;

    InitWindow(window_width, window_height, "ave - Another Video Editor");

    Rectangle videoAreaBorder =
                  (Rectangle){.x = (int)(window_width / 3),
                              .y = 10,
                              .width = (int)((2 * window_width) / 3) - 20,
                              .height = window_height - 100},
              videoArea =
                  (Rectangle){.x = (int)(window_width / 3) + 2,
                              .y = 12,
                              .width = (int)((2 * window_width) / 3) - 24,
                              .height = window_height - 104},
              playButton =
                  (Rectangle){.x = (int)(window_width / 3 + window_width / 4),
                              .y = videoAreaBorder.height + 30,
                              .width = 100,
                              .height = 35},
              resetButton =
                  (Rectangle){.x = (int)(window_width / 3 + window_width / 4) +
                                   playButton.width + 10,
                              .y = videoAreaBorder.height + 30,
                              .width = 60,
                              .height = 35},
              exportButton =
                  (Rectangle){.x = (int)(window_width / 3 + window_width / 4) +
                                   resetButton.width + playButton.width + 20,
                              .y = videoAreaBorder.height + 30,
                              .width = 110,
                              .height = 35},
              mediaArea = (Rectangle){.x = 10,
                                      .y = 10,
                                      .width = (int)(window_width / 3) - 20,
                                      .height = window_height - 20};

    Image img = GenImageColor(videoArea.width, videoArea.height, BLACK);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    AppState *state = (AppState *)malloc(sizeof(AppState));
    state->media_count = 0;
    state->current_media_idx = 0;

    state->video_area_width = videoArea.width;
    state->video_area_height = videoArea.height;
    state->video_destination_fmt = AV_PIX_FMT_RGBA;

    state->now = GetTime();
    state->elapsed = 0;
    state->target_fps = 25;

    while (!WindowShouldClose()) {
        PollInputEvents();

        if (IsFileDropped()) {
            FilePathList dropped_files = LoadDroppedFiles();

            for (int i = 0; i < (int)dropped_files.count; i++) {
                MediaStateWrapper *media_state =
                    (MediaStateWrapper *)malloc(sizeof(MediaStateWrapper));
                if (!media_state) {
                    printf("Failed to allocate media state\n");
                    continue;
                }

                media_state->media = media_alloc();
                if (!media_state->media) {
                    printf("Failed to allocate media\n");
                    continue;
                }

                strcpy(media_state->media->filename, dropped_files.paths[i]);

                if ((ret =
                         media_init(media_state->media, state->video_area_width,
                                    state->video_area_height,
                                    state->video_destination_fmt)) < 0) {
                    TraceLog(LOG_ERROR, "Failed to initialize media: %d", ret);
                    continue;
                }

                media_state->is_playing = 0;
                media_state->end_of_file = 0;

                state->medias[state->media_count] = media_state;
                state->media_count++;
            }

            UnloadDroppedFiles(dropped_files);
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && state->media_count > 0) {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, playButton)) {
                state->medias[state->current_media_idx]->is_playing =
                    !state->medias[state->current_media_idx]->is_playing;
                state->now = GetTime();
            } else if (CheckCollisionPointRec(mouse, resetButton)) {
                // Seek to the beginning of the video
            } else if (CheckCollisionPointRec(mouse, exportButton)) {
                // Export video
            }
        }

        if (state->media_count > 0 &&
            state->medias[state->current_media_idx]->is_playing) {
            MediaStateWrapper *media_state =
                state->medias[state->current_media_idx];
            if ((ret = media_read_frame(media_state->media)) < 0) {
                if (ret == MEDIA_ERR_EOF) {
                    state->medias[state->current_media_idx]->end_of_file = 1;
                    state->medias[state->current_media_idx]->is_playing = 0;
                    continue;
                } else {
                    TraceLog(LOG_ERROR, "Failed to read frame: %d", ret);
                    break;
                }
            }

            if ((ret = media_decode(media_state->media)) < 0) {
                TraceLog(LOG_ERROR, "Failed to decode frame: %d", ret);
                break;
            }

            Node *node = fq_dequeue(media_state->media->queue);
            if (!node) {
                TraceLog(LOG_ERROR, "Failed to dequeue frame");
                break;
            }

            if (node->type == FRAME_TYPE_VIDEO) {
                UpdateTexture(tex, node->frame->data[0]);
            } else {
                // TraceLog(LOG_INFO, "Decoded audio frame: %d samples",
                //          node->frame->nb_samples);
            }

            node_free(node);
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawRectangleLinesEx(mediaArea, 2, GRAY);
        if (state->media_count == 0) {
            DrawText("Drop media files here!", mediaArea.x + 15,
                     mediaArea.height / 2, 25, GRAY);
        } else {
            for (int i = 0; i < state->media_count; i++) {
                DrawRectangle(mediaArea.x, mediaArea.y + 35 * i,
                              mediaArea.width, 35,
                              i == state->current_media_idx
                                  ? SKYBLUE
                                  : (i % 2 == 0 ? GRAY : LIGHTGRAY));
                DrawText(basename(state->medias[i]->media->filename),
                         mediaArea.x + 10, mediaArea.y + 35 * i, 20,
                         i == state->current_media_idx ? BLUE : DARKGRAY);
            }
        }

        DrawRectangleLinesEx(videoAreaBorder, 2, GRAY);
        DrawRectangleRec(videoArea, WHITE);

        if (state->media_count > 0 &&
            state->medias[state->current_media_idx]->end_of_file) {
            DrawText("End of file", videoArea.x + 10, videoArea.y + 10, 20,
                     RED);
        } else {
            DrawTexture(tex, videoArea.x, videoArea.y, WHITE);
        }

        DrawRectangleRec(playButton, LIGHTGRAY);
        DrawText("Play/Pause", playButton.x + 5, playButton.y + 10, 15, GRAY);

        DrawRectangleRec(resetButton, LIGHTGRAY);
        DrawText("Reset", resetButton.x + 5, resetButton.y + 10, 15, GRAY);

        DrawRectangleRec(exportButton, LIGHTGRAY);
        DrawText("Export (.mp4)", exportButton.x + 5, exportButton.y + 10, 15,
                 GRAY);

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

            TraceLog(LOG_INFO,
                     "Stream index: %d, Timebase: %f, PTS: %f, PTS time: %f, "
                     "Wait time: %f, Now: %f, Elapsed: %f",
                     stream_index, timebase, pts, pts_time, wait_time,
                     state->now, state->elapsed);
            if (wait_time > 0) {
                WaitTime(wait_time);
            }
        } else {
            if (state->elapsed < 1.0 / state->target_fps) {
                WaitTime(1.0 / state->target_fps - state->elapsed);
            }
        }

        // state->now = GetTime();
    }

    app_state_free(state);
    UnloadTexture(tex);
    CloseWindow();
}
