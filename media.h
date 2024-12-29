// In this header, we define everything we need in order to open a media
// container, and read frames from it. It should contain all the necessary
// FFMPEG contexts and variables that allows us to safely decode audio+video.
#ifndef MEDIA_H
#define MEDIA_H

#include "common.h"
#include "queue.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

enum MediaError {
  MEDIA_ERR_INTERNAL = -1,
  MEDIA_ERR_LIBAV = -2,
  MEDIA_ERR_NO_STREAM = -3,
  MEDIA_ERR_EOF = -4,
};

typedef struct Media {
  char *filename;

  AVFormatContext *fmt_ctx;

  int video_stream_idx, audio_stream_idx;
  AVCodecContext *audio_ctx, *video_ctx;
  struct SwsContext *sws_ctx;

  // These variables will be used to scale the video frames.
  int dst_frame_w, dst_frame_h;
  enum AVPixelFormat dst_frame_fmt;

  FrameQueue *queue;

  AVPacket *pkt;
} Media;

Media *media_alloc();

int media_open_context(Media *media, enum AVMediaType type);
int media_init(Media *media, int dst_frame_w, int dst_frame_h, enum AVPixelFormat dst_frame_fmt);

// Sends the packet to the decoder appropriate.
int media_read_frame(Media *media);
int media_decode(Media *media);

void media_free(Media *media);

#endif // MEDIA_H
