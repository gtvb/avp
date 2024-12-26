#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#include "media.h"
#include "raylib.h"

#define MAX_MEDIA_QUANTITY 5

typedef struct EditorState {
  int video_area_width, video_area_height;

  Media *medias[MAX_MEDIA_QUANTITY];
  int media_count;
  int current_media_idx;
  int current_media_first_frame;

  int current_media_playing;
  int current_media_ended;
  int current_media_reset;
} EditorState;

EditorState *state_alloc(int video_area_width, int video_area_height) {
  EditorState *es = (EditorState *)malloc(sizeof(EditorState));

  es->current_media_idx = 0;
  es->current_media_first_frame = 1;
  es->current_media_ended = 0;
  es->current_media_playing = 0;
  es->current_media_reset = 0;
  es->media_count = 0;

  es->video_area_width = video_area_width;
  es->video_area_height = video_area_height;

  for (int i = 0; i < MAX_MEDIA_QUANTITY; i++) {
    es->medias[i] = NULL;
  }

  return es;
}

int state_load_media(EditorState *es, char *droppedFilename) {
  Media *m = media_alloc(es->video_area_width, es->video_area_height);
  if (!m) {
    return -1;
  }

  strcpy(m->filename, droppedFilename);

  if (media_init(m) < 0) {
    return -1;
  }

  es->medias[es->media_count] = m;
  es->media_count++;

  return 0;
}

void state_free(EditorState *es) {
  for (int i = 0; i < es->media_count; i++) {
    if (es->medias[i])
      media_free(es->medias[i]);
  }

  free(es);
}

int main(void) {
  const int window_width = 1280;
  const int window_height = 720;

  InitWindow(window_width, window_height, "ave - Another Video Editor");

  Rectangle videoAreaBorder =
                (Rectangle){.x = (int)(window_width / 3),
                            .y = 10,
                            .width = (int)((2 * window_width) / 3) - 20,
                            .height = window_height - 100},
            videoArea = (Rectangle){.x = (int)(window_width / 3) + 2,
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
  Texture tex = LoadTextureFromImage(img);
  UnloadImage(img);

  EditorState *state = state_alloc(videoArea.width, videoArea.height);
  if (!state) {
    perror("state_alloc");
    exit(1);
  }

  SetTargetFPS(30);
  while (!WindowShouldClose()) {
    if (IsFileDropped() && state->media_count + 1 < MAX_MEDIA_QUANTITY) {
      FilePathList droppedFiles = LoadDroppedFiles();
      for (int i = 0; i < (int)droppedFiles.count; i++) {
        state_load_media(state, droppedFiles.paths[i]);
      }

      UnloadDroppedFiles(droppedFiles);
    }

    if (CheckCollisionPointRec(GetMousePosition(), playButton)) {
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        state->current_media_playing = !state->current_media_playing;
        TraceLog(LOG_INFO, "Playing: %d\n", state->current_media_playing);
      }
    }

    if (CheckCollisionPointRec(GetMousePosition(), resetButton)) {
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        state->current_media_playing = 0;
        state->current_media_reset = 1;
        state->current_media_first_frame = 1;
        TraceLog(LOG_INFO, "Reset: %d\n", state->current_media_reset);
      }
    }

    if (IsKeyPressed(KEY_DOWN)) {
      state->current_media_idx =
          (state->current_media_idx + 1) % state->media_count;
      state->current_media_first_frame = 1;
    }

    if (IsKeyPressed(KEY_UP)) {
      state->current_media_idx =
          (state->current_media_idx - 1) % state->media_count;
      state->current_media_first_frame = 1;
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawRectangleLinesEx(mediaArea, 2, GRAY);
    if (state->media_count == 0) {
      DrawText("Drop media files here!", mediaArea.x + 15, mediaArea.height / 2,
               25, GRAY);
    } else {
      for (int i = 0; i < state->media_count; i++) {
        DrawRectangle(mediaArea.x, mediaArea.y + 35 * i, mediaArea.width, 35,
                      i == state->current_media_idx
                          ? SKYBLUE
                          : (i % 2 == 0 ? GRAY : LIGHTGRAY));
        DrawText(basename(state->medias[i]->filename), mediaArea.x + 10,
                 mediaArea.y + 35 * i, 20,
                 i == state->current_media_idx ? BLUE : DARKGRAY);
      }
    }

    if (state->current_media_playing) {
      if (media_read_frame(state->medias[state->current_media_idx]) < 0) {
        TraceLog(LOG_ERROR, "Could not read frame\n");
      };

      int index;
      if ((index = media_decode_frames(
               state->medias[state->current_media_idx])) < 0) {
        TraceLog(LOG_ERROR, "Could not decode frame(s)\n");
      };

      if (index ==
          state->medias[state->current_media_idx]->video_stream_index) {
        UpdateTexture(tex,
                      state->medias[state->current_media_idx]->frame->data);

        TraceLog(LOG_INFO, "SOme data: %d, %d\n",
                 state->medias[state->current_media_idx]->frame->data[0][0],
                 state->medias[state->current_media_idx]->frame->data[0][1]);
        TraceLog(LOG_INFO, "Got video frame\n");
      } else {
        TraceLog(LOG_INFO, "Got audio frame\n");
      }
    }

    DrawRectangleLinesEx(videoAreaBorder, 2, GRAY);
    DrawRectangleRec(videoArea, WHITE);

    DrawTexture(tex, videoArea.x, videoArea.y, WHITE);

    DrawRectangleRec(playButton, LIGHTGRAY);
    DrawText("Play/Pause", playButton.x + 5, playButton.y + 10, 15, GRAY);

    DrawRectangleRec(resetButton, LIGHTGRAY);
    DrawText("Reset", resetButton.x + 5, resetButton.y + 10, 15, GRAY);

    DrawRectangleRec(exportButton, LIGHTGRAY);
    DrawText("Export (.mp4)", exportButton.x + 5, exportButton.y + 10, 15,
             GRAY);

    EndDrawing();
  }

  state_free(state);
  UnloadTexture(tex);
  CloseWindow();
}
