#include "media.h"

Media *media_alloc() {
    Media *media = malloc(sizeof(Media));
    if (!media) {
        return NULL;
    }

    media->filename = malloc(1024 * sizeof(char));
    media->fmt_ctx = NULL;
    media->audio_ctx = NULL;
    media->video_ctx = NULL;
    media->video_stream_idx = -1;
    media->audio_stream_idx = -1;
    media->sws_ctx = NULL;
    media->dst_frame_w = 0;
    media->dst_frame_h = 0;
    media->dst_frame_fmt = AV_PIX_FMT_NONE;
    media->pkt = NULL;
    media->queue = NULL;

    return media;
}

int media_open_context(Media *media, enum AVMediaType type) {
    if (!media || !media->filename) {
        printf("media_open_context: media or filename is NULL\n");
        return MEDIA_ERR_INTERNAL;
    }

    AVCodec *codec = NULL;
    AVCodecParameters *codec_params = NULL;
    int stream_index = -1;

    for (int i = 0; i < media->fmt_ctx->nb_streams; i++) {
        if (media->fmt_ctx->streams[i]->codecpar->codec_type == type) {
            codec_params = media->fmt_ctx->streams[i]->codecpar;
            stream_index = i;
            break;
        }
    }

    if (stream_index == -1) {
        printf("media_open_context: no stream found\n");
        return MEDIA_ERR_NO_STREAM;
    }

    codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        printf("media_open_context: avcodec_find_decoder failed\n");
        return MEDIA_ERR_LIBAV;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        return MEDIA_ERR_LIBAV;
    }

    int ret = avcodec_parameters_to_context(codec_ctx, codec_params);
    if (ret < 0) {
        return MEDIA_ERR_LIBAV;
    }

    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        return MEDIA_ERR_LIBAV;
    }

    if (type == AVMEDIA_TYPE_AUDIO) {
        media->audio_ctx = codec_ctx;
        media->audio_stream_idx = stream_index;
    } else if (type == AVMEDIA_TYPE_VIDEO) {
        media->video_ctx = codec_ctx;
        media->video_stream_idx = stream_index;
    }

    return 0;
}

int media_init(Media *media, int dst_frame_w, int dst_frame_h,
               enum AVPixelFormat dst_frame_fmt) {
    if (!media) {
        printf("media_init: media is NULL\n");
        return MEDIA_ERR_INTERNAL;
    }

    int ret = avformat_open_input(&media->fmt_ctx, media->filename, NULL, NULL);
    if (ret < 0) {
        printf("media_init: avformat_open_input failed: %s\n", av_err2str(ret));
        return MEDIA_ERR_LIBAV;
    }

    ret = avformat_find_stream_info(media->fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "media_init: avformat_find_stream_info failed: %s\n",
                av_err2str(ret));
        return MEDIA_ERR_LIBAV;
    }

    media->queue = fq_alloc();
    if (!media->queue) {
        printf("media_init: fq_alloc failed\n");
        return MEDIA_ERR_INTERNAL;
    }

    // Try to open the audio and video contexts. If they fail, it just means
    // that this media does not contain and audio or video stream, but we can
    // keep going.
    ret = media_open_context(media, AVMEDIA_TYPE_AUDIO);
    if (ret < 0 && ret != MEDIA_ERR_NO_STREAM) {
        return ret;
    }

    ret = media_open_context(media, AVMEDIA_TYPE_VIDEO);
    if (ret < 0 && ret != MEDIA_ERR_NO_STREAM) {
        return ret;
    }

    if (media->video_ctx) {
        media->sws_ctx =
            sws_getContext(media->video_ctx->width, media->video_ctx->height,
                           media->video_ctx->pix_fmt, dst_frame_w, dst_frame_h,
                           dst_frame_fmt, SWS_BILINEAR, NULL, NULL, NULL);

        if (!media->sws_ctx) {
            printf("media_init: sws_getContext failed\n");
            return MEDIA_ERR_LIBAV;
        }
    }

    media->dst_frame_w = dst_frame_w;
    media->dst_frame_h = dst_frame_h;
    media->dst_frame_fmt = dst_frame_fmt;

    media->pkt = av_packet_alloc();
    if (!media->pkt) {
        printf("media_init: av_packet_alloc failed\n");
        return MEDIA_ERR_LIBAV;
    }

    return 0;
}

int media_read_frame(Media *media) {
    if (!media) {
        printf("media_read_frame: media is NULL\n");
        return MEDIA_ERR_INTERNAL;
    }

    int ret = av_read_frame(media->fmt_ctx, media->pkt);
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            printf("media_read_frame: end of file\n");
            return MEDIA_ERR_EOF;
        }
        printf("media_read_frame: av_read_frame failed: %s\n", av_err2str(ret));
        return MEDIA_ERR_LIBAV;
    }

    return 0;
}

int media_decode(Media *media) {
    if (!media) {
        printf("media_decode: media is NULL\n");
        return MEDIA_ERR_INTERNAL;
    }

    int is_video = 0;
    AVCodecContext *codec_ctx = NULL;
    if (media->pkt->stream_index == media->video_stream_idx) {
        codec_ctx = media->video_ctx;
        is_video = 1;
    } else if (media->pkt->stream_index == media->audio_stream_idx) {
        codec_ctx = media->audio_ctx;
    } else {
        return MEDIA_ERR_INTERNAL;
    }

    int ret = avcodec_send_packet(codec_ctx, media->pkt);
    if (ret < 0) {
        printf("media_decode: avcodec_send_packet failed: %s\n",
               av_err2str(ret));
        return MEDIA_ERR_LIBAV;
    }

    while (ret >= 0) {
        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            printf("media_decode: av_frame_alloc failed\n");
            return MEDIA_ERR_LIBAV;
        }

        // Receive the decoded frames (or frame).
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            printf("media_decode: avcodec_receive_frame failed: %s\n",
                   av_err2str(ret));
            return MEDIA_ERR_LIBAV;
        }

        // If we are decoding video, we need to scale the frame.
        if (is_video) {
            AVFrame *scaled_frame = av_frame_alloc();
            if (!scaled_frame) {
                printf("media_decode: av_frame_alloc failed\n");
                return MEDIA_ERR_LIBAV;
            }

            scaled_frame->width = media->dst_frame_w;
            scaled_frame->height = media->dst_frame_h;
            scaled_frame->format = media->dst_frame_fmt;

            ret = av_frame_get_buffer(scaled_frame, 32);
            if (ret < 0) {
                fprintf(stderr,
                        "media_decode: av_frame_get_buffer failed: %s\n",
                        av_err2str(ret));
                return MEDIA_ERR_LIBAV;
            }

            ret = sws_scale(media->sws_ctx, frame->data, frame->linesize, 0,
                            frame->height, scaled_frame->data,
                            scaled_frame->linesize);
            if (ret < 0) {
                printf("media_decode: sws_scale failed\n");
                return MEDIA_ERR_LIBAV;
            }

            av_frame_free(&frame);
            frame = scaled_frame;

            fq_enqueue(media->queue, frame, FRAME_TYPE_VIDEO);
        } else {
            fq_enqueue(media->queue, frame, FRAME_TYPE_AUDIO);
        }
    }
}

void media_free(Media *media) {
    if (!media) {
        return;
    }

    if (media->fmt_ctx) {
        avformat_close_input(&media->fmt_ctx);
    }

    if (media->audio_ctx) {
        avcodec_free_context(&media->audio_ctx);
    }

    if (media->video_ctx) {
        avcodec_free_context(&media->video_ctx);
    }

    if (media->sws_ctx) {
        sws_freeContext(media->sws_ctx);
    }

    if (media->pkt) {
        av_packet_free(&media->pkt);
    }

    if (media->queue) {
        fq_free(media->queue);
    }

    free(media->filename);
    free(media);
}