#include "media.h"

Media *media_alloc() {
    Media *media = malloc(sizeof(Media));
    if (!media) {
        return NULL;
    }

    media->filename = malloc(1024 * sizeof(char));
    media->formatted_duration = malloc(10 * sizeof(char));
    media->formatted_position = malloc(10 * sizeof(char));

    if (!media->filename || !media->formatted_duration ||
        !media->formatted_position) {
        free(media->filename);
        free(media->formatted_duration);
        free(media->formatted_position);
        free(media);
        return NULL;
    }

    media->fmt_ctx = NULL;
    media->audio_ctx = NULL;
    media->video_ctx = NULL;
    media->sws_ctx = NULL;
    media->swr_ctx = NULL;

    media->video_stream_idx = -1;
    media->audio_stream_idx = -1;

    media->dst_frame_w = 0;
    media->dst_frame_h = 0;
    media->dst_frame_fmt = AV_PIX_FMT_NONE;

    media->pkt = NULL;
    media->queue = NULL;

    media->position = 0;

    return media;
}

int media_open_context(Media *media, enum AVMediaType type) {
    if (!media || !media->filename) {
        printf("media_open_context: media or filename is NULL\n");
        return MEDIA_ERR_INTERNAL;
    }

    const AVCodec *codec = NULL;
    AVCodecParameters *codec_params = NULL;
    int stream_index = -1;

    for (int i = 0; i < (int)media->fmt_ctx->nb_streams; i++) {
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

    // For audio: For planar sample formats, each audio channel is in a separate
    // data plane, and linesize is the buffer size, in bytes, for a single
    // plane. All data planes must be the same size. For packed sample formats,
    // only the first data plane is used, and samples for each channel are
    // interleaved. In this case, linesize is the buffer size, in bytes, for the
    // 1 plane.
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

    if (media->audio_ctx) {
        SwrContext *swr = swr_alloc();
        if (!swr) {
            printf("media_init: swr_alloc failed\n");
            return MEDIA_ERR_LIBAV;
        }

        AVChannelLayout in_ch_layout;
        if (av_channel_layout_copy(&in_ch_layout,
                                   &media->audio_ctx->ch_layout) < 0) {
            printf("media_init: av_channel_layout_copy failed\n");
            return MEDIA_ERR_LIBAV;
        }

        ret = swr_alloc_set_opts2(
            &swr,
            &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO,  // out_ch_layout
            AV_SAMPLE_FMT_FLT,                           // out_sample_fmt
            media->audio_ctx->sample_rate,               // out_sample_rate
            &in_ch_layout,                               // in_ch_layout
            media->audio_ctx->sample_fmt,                // in_sample_fmt
            media->audio_ctx->sample_rate,               // in_sample_rate
            0,                                           // log_offset
            NULL);                                       // log_ctx

        if (ret < 0) {
            printf("media_init: swr_alloc_set_opts2 failed\n");
            return MEDIA_ERR_LIBAV;
        }

        if (swr_init(swr) < 0) {
            printf("media_init: swr_init failed\n");
            return MEDIA_ERR_LIBAV;
        }

        media->swr_ctx = swr;
    }

    media->dst_frame_w = dst_frame_w;
    media->dst_frame_h = dst_frame_h;
    media->dst_frame_fmt = dst_frame_fmt;

    media->pkt = av_packet_alloc();
    if (!media->pkt) {
        printf("media_init: av_packet_alloc failed\n");
        return MEDIA_ERR_LIBAV;
    }

    // Set the duration of the stream here.
    media_get_formatted_time(media, media->fmt_ctx->duration, AV_TIME_BASE,
                             media->formatted_duration);

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

    // Update the position at container scale
    media->position = media->pkt->pts;

    int64_t timebase;
    if (media->pkt->stream_index == media->video_stream_idx) {
        timebase =
            media->fmt_ctx->streams[media->video_stream_idx]->time_base.den;
    } else {
        timebase =
            media->fmt_ctx->streams[media->audio_stream_idx]->time_base.den;
    }

    if (media_get_formatted_time(media, media->position, timebase,
                                 media->formatted_position) < 0) {
        printf("media_read_frame: media_get_formatted_time failed\n");
        return MEDIA_ERR_INTERNAL;
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
        if (ret == AVERROR(EAGAIN)) {
            printf("media_decode: avcodec_receive_frame returned EAGAIN: %s\n",
                   av_err2str(ret));
            av_frame_free(&frame);
            return MEDIA_ERR_MORE_DATA;
        } else if (ret == AVERROR_EOF) {
            printf("media_decode: avcodec_receive_frame returned EOF: %s\n",
                   av_err2str(ret));
            av_frame_free(&frame);
            return MEDIA_ERR_EOF;
        } else if (ret < 0) {
            printf("media_decode: avcodec_receive_frame failed: %s\n",
                   av_err2str(ret));
            return MEDIA_ERR_LIBAV;
        }

        if (is_video) {
            AVFrame *scaled_frame = av_frame_alloc();
            if (!scaled_frame) {
                printf("media_decode: av_frame_alloc failed\n");
                return MEDIA_ERR_LIBAV;
            }

            if (av_image_alloc(scaled_frame->data, scaled_frame->linesize,
                               media->dst_frame_w, media->dst_frame_h,
                               media->dst_frame_fmt, 1) < 0) {
                printf("media_decode: av_image_alloc failed\n");
                return MEDIA_ERR_LIBAV;
            }

            ret = sws_scale(media->sws_ctx, (const uint8_t *const *)frame->data,
                            frame->linesize, 0, frame->height,
                            scaled_frame->data, scaled_frame->linesize);
            if (ret < 0) {
                printf("media_decode: sws_scale failed\n");
                return MEDIA_ERR_LIBAV;
            }

            av_frame_free(&frame);
            frame = scaled_frame;

            fq_enqueue(media->queue, frame, FRAME_TYPE_VIDEO);
        } else {
            AVFrame *converted_frame = av_frame_alloc();
            if (!converted_frame) {
                printf("media_decode: av_frame_alloc failed\n");
                return MEDIA_ERR_LIBAV;
            }

            converted_frame->format = AV_SAMPLE_FMT_FLT;
            converted_frame->sample_rate = media->audio_ctx->sample_rate;
            converted_frame->nb_samples = frame->nb_samples;

            av_channel_layout_default(&converted_frame->ch_layout, 2);

            if ((ret = av_frame_get_buffer(converted_frame, 0)) < 0) {
                printf("media_decode: av_frame_get_buffer failed: %s\n",
                       av_err2str(ret));
                return MEDIA_ERR_LIBAV;
            }

            ret = swr_convert(media->swr_ctx, converted_frame->data,
                              converted_frame->nb_samples,
                              (const uint8_t **)frame->data, frame->nb_samples);

            if (ret < 0) {
                printf("media_decode: swr_convert failed\n");
                return MEDIA_ERR_LIBAV;
            }

            av_frame_copy_props(converted_frame, frame);
            av_frame_free(&frame);
            frame = converted_frame;

            fq_enqueue(media->queue, frame, FRAME_TYPE_AUDIO);
        }
    }

    return 0;
}

int media_seek(Media *media, int64_t incr, enum SeekDirection direction) {
    if (!media) {
        printf("media_seek: media is NULL\n");
        return MEDIA_ERR_INTERNAL;
    }

    int flags = 0;
    if (direction == SEEK_BACKWARD) {
        flags |= AVSEEK_FLAG_BACKWARD;
    }

    int stream_index;
    if (media->pkt->stream_index == media->video_stream_idx) {
        stream_index = media->video_stream_idx;
    } else {
        stream_index = media->audio_stream_idx;
    }

    int64_t timestamp = media->position + incr > 0 ? media->position + incr : 0;
    media->position = timestamp;

    int64_t target =
        av_rescale_q(timestamp, AV_TIME_BASE_Q,
                     media->fmt_ctx->streams[stream_index]->time_base);

    int ret =
        av_seek_frame(media->fmt_ctx, media->video_stream_idx, target, flags);
    if (ret < 0) {
        printf("media_seek: av_seek_frame failed: %s\n", av_err2str(ret));
        return MEDIA_ERR_LIBAV;
    }

    avcodec_flush_buffers(media->video_ctx);
    avcodec_flush_buffers(media->audio_ctx);

    return 0;
}

int media_get_formatted_time(Media *media, int64_t timestamp, int64_t timebase,
                             char *formatted_time) {
    if (!media) {
        printf("media_get_formatted_time: media is NULL\n");
        return -1;
    }

    int64_t seconds = timestamp / timebase;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;

    snprintf(formatted_time, 10, "%02lld:%02lld:%02lld", hours, minutes % 60,
             seconds % 60);
    return 0;
}

void media_free(Media *media) {
    if (!media) {
        return;
    }

    if (media->formatted_duration) {
        free(media->formatted_duration);
    }

    if (media->formatted_position) {
        free(media->formatted_position);
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

    if (media->swr_ctx) {
        swr_free(&media->swr_ctx);
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