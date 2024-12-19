#include <media.h>

Media *alloc_media(const char *filename) {
    Media *media = (Media *)malloc(sizeof(Media));
    media->filename = filename;
    media->fmt_ctx = NULL;

    for(int i = 0; i < 2; i++) {
        media->ctxs[i] = NULL;
    }

    media->pkt = av_packet_alloc();
    media->frame = av_frame_alloc();
    media->dst_frame = av_frame_alloc();

    media->sws_ctx = sws_getContext(
        media->frame->width, media->frame->height, media->ctxs[0]->pix_fmt, media->frame->width,
        media->frame->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

    return media;
}

int open_contexts(Media *media) {
    for(int i = 0; i < media->fmt_ctx->nb_streams; i++) {
        AVStream *stream = media->fmt_ctx->streams[i];
        const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);

        if(!codec) {
            fprintf(stderr, "Failed to find decoder for stream %d\n", i);
            return -1;
        }
        AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
        if(!codec_ctx) {
            fprintf(stderr, "Failed to allocate decoder context for stream %d\n", i);
            return -1;
        }
        if(avcodec_parameters_to_context(codec_ctx, stream->codecpar) < 0) {
            fprintf(stderr, "Failed to copy codec parameters to decoder context for stream %d\n", i);
            return -1;
        }
        if(avcodec_open2(codec_ctx, codec, NULL) < 0) {
            fprintf(stderr, "Failed to open codec for stream %d\n", i);
            return -1;
        }
        media->ctxs[i] = codec_ctx;
    }
}

int init_media(Media *media) {
    // Setup the format context with the input info
    if (avformat_open_input(&media->fmt_ctx, media->filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", media->filename);
        return -1;
    }
    if (avformat_find_stream_info(media->fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return -1;
    }

    // setup decoder contexts to get audio and video frames
    if(open_contexts(media) < 0) {
        fprintf(stderr, "Failed to setup decoder contexts\n");
        return -1;
    }

    if (av_image_alloc((uint8_t **)&media->dst_frame->data, media->dst_frame->linesize,
                       media->frame->width, media->frame->height, AV_PIX_FMT_RGB24, 0) < 0) {
        fprintf(stderr, "Failed to allocate destination frame buffer\n");
        return -1;
    }

    return 0;
}

int decode_packet(Media *media) {
    // Read a frame from the format context into a packet
    if(av_read_frame(media->fmt_ctx, media->pkt) < 0) {
        return -1;
    }

    // Send the packet to an appropriate decoder context
    int ctx_idx = media->pkt->stream_index;
    if(avcodec_send_packet(media->ctxs[ctx_idx], media->pkt) < 0) {
        return -1;
    }

    if (avcodec_receive_frame(media->ctxs[ctx_idx], media->frame) < 0) {
        return -1;
    }

    // if frame is video, convert it to RGB24
    if(is_frame_video(media)) {
        sws_scale(media->sws_ctx, (const uint8_t *const *)media->frame->data, media->frame->linesize,
                  0, media->frame->height, media->dst_frame->data, media->dst_frame->linesize);
    }

    return 0;
}

int is_frame_video(Media *media) {
    return media->pkt->stream_index == 0;
}

void free_media(Media *media) {
    avformat_close_input(&media->fmt_ctx);

    sws_freeContext(media->sws_ctx);
    avcodec_free_context(&media->ctxs[0]);
    avcodec_free_context(&media->ctxs[1]);

    av_packet_free(&media->pkt);
    av_frame_free(&media->frame);

    av_freep(&media->dst_frame->data[0]);
    av_frame_free(&media->dst_frame);

    free(media);
}