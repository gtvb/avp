// In this header, we define everything we need in order to open a media
// container, and read frames from it. It should contain all the necessary
// FFMPEG contexts and variables that allows us to safely decode audio+video.
#ifndef MEDIA_H
#define MEDIA_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#define MAX_FILEPATH_SIZE 1024
#define MAX_CONTEXTS 3

typedef struct Media {
  char *filename;

  AVFormatContext *fmt_ctx;

  int nb_streams, video_stream_index, audio_stream_index;
  AVCodecContext *ctxs[MAX_CONTEXTS];
  AVStream *streams[MAX_CONTEXTS];

  // This is an auxiliary variable to read all the frames from the container
  // wheter they are audio or video frames.
  AVPacket *pkt;

  // Frame for reading purposes
  AVFrame *frame;

  // TODO: Here, we will implement storage for audio and video streams

  // This is the context that we're gonna use to convert the original pixel
  // format on the frame to the desired format
  struct SwsContext *sws_ctx;
} Media;

Media *media_alloc();

int media_open_contexts(Media *media);
int media_init(Media *media, int, int, enum AVPixelFormat);

AVCodecContext *media_get_video_ctx(Media *media);
AVCodecContext *media_get_audio_ctx(Media *media);

int media_read_frame(Media *media);
int media_seek(Media *media, int64_t target);

int media_decode_frames(Media *media);

void media_free(Media *media);

#endif // MEDIA_H
