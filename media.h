// In this header, we define everything we need in order to open a media
// container, and read frames from it. It should contain all the necessary
// FFMPEG contexts and variables that allows us to safely decode audio+video.
#ifndef MEDIA_H
#define MEDIA_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include "common.h"
#include "queue.h"

#define OUT_SAMPLE_FMT AV_SAMPLE_FMT_FLT

enum MediaError {
    MEDIA_ERR_INTERNAL = -1,
    MEDIA_ERR_LIBAV = -2,
    MEDIA_ERR_NO_STREAM = -3,
    MEDIA_ERR_EOF = -4,
    MEDIA_ERR_MORE_DATA = -5,
};

enum SeekDirection {
    SEEK_FORWARD,
    SEEK_BACKWARD,
};

typedef struct Media {
    char *filename;

    AVFormatContext *fmt_ctx;

    int video_stream_idx, audio_stream_idx;
    AVCodecContext *audio_ctx, *video_ctx;

    // Audio and video conversion contexts.
    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;

    // These variables will be used to scale the video frames.
    int dst_frame_w, dst_frame_h;
    enum AVPixelFormat dst_frame_fmt;

    // This allows us to correctly seek frames on the media.
    int64_t position;

    // Duration times in string format
    char *formatted_duration;
    char *formatted_position;

    // This queue stores the audio+video frames so they can be processed later.
    FrameQueue *queue;

    // Auxiliary context used to decode packets.
    AVPacket *pkt;
} Media;

Media *media_alloc();

int media_open_context(Media *media, enum AVMediaType type);
int media_init(Media *media, int dst_frame_w, int dst_frame_h,
               enum AVPixelFormat dst_frame_fmt);

// Sends the packet to the decoder appropriate.
int media_read_frame(Media *media);
int media_decode(Media *media);
int media_seek(Media *media, int64_t incr, enum SeekDirection direction);

int media_get_formatted_time(Media *media, int64_t timestamp, int64_t timebase,
                             char *formatted_time);

void media_free(Media *media);

#endif  // MEDIA_H
