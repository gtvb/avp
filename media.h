// In this header, we define everything we need in order to open a media
// container, and read frames from it. It should contain all the necessary
// FFMPEG contexts and variables that allows us to safely decode audio+video.
#ifndef MEDIA_H
#define MEDIA_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#define MAX_FILEPATH_RECORDED 4096
#define MAX_FILEPATH_SIZE 2048

typedef struct Media {
  char *filename;
  // Contains info about the image (streams, formats, dimensions, etc.)
  AVFormatContext *fmt_ctx;
  AVCodecContext *video_codec, *audio_codec;

  struct AVPacket attached_pic;

  int video_stream_index, audio_stream_index;
  int video_area_width, video_area_height;

  // This is an auxiliary variable to read all the frames from the container
  // wheter they are audio or video frames.
  AVPacket *pkt;

  // Frame for reading purposes
  AVFrame *frame, *dst_frame;

  // This is the context that we're gonna use to convert the original pixel
  // format on the frame to the desired format
  struct SwsContext *sws_ctx;
} Media;

Media *media_alloc(int video_area_width, int video_area_height);

AVCodecContext *media_open_context(Media *media, enum AVMediaType type);
int media_init(Media *media);
int media_read_frame(Media *media);
int media_decode_frames(Media *media);

void media_free(Media *media);

#endif // MEDIA_H
