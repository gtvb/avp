// In this header, we define everything we need in order to open a media container, 
// and read frames from it. It should contain all the necessary FFMPEG contexts and variables
// that allows us to safely decode audio+video.
#ifndef MEDIA_H
#define MEDIA_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

typedef struct Media {
    const char *filename;
    // Contains info about the image (streams, formats, dimensions, etc.) 
    AVFormatContext *fmt_ctx;
    // TODO: I suppose this can be bigger than 2
    AVCodecContext *ctxs[2];

    // This is an auxiliary variable to read all the frames from the container
    // wheter they are audio or video frames.
    AVPacket *pkt;
    // These are the frames that we're gonna use to store the decoded data.
    // The dst_frame will contain the RGB data, and the frame will contain
    // the original decoded data from the container.
    AVFrame *frame, *dst_frame;

    // This is the context that we're gonna use to convert the original pixel format on 
    // the frame to the RGB24 format.
    struct SwsContext *sws_ctx;
} Media;

Media *alloc_media(const char *filename);

int open_contexts(Media *media);
int init_media(Media *media);
int decode_packet(Media *media);
int is_frame_video(Media *media);

void free_media(Media *media);

#endif // MEDIA_H
