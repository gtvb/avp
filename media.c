#include "media.h"

Media *media_alloc() {
  Media *media = (Media *)malloc(sizeof(Media));

  media->filename = (char *)malloc(sizeof(char) * MAX_FILEPATH_SIZE);
  if (!media->filename)
    return NULL;

  media->fmt_ctx = NULL;
  media->nb_streams = 0;

  media->audio_stream_index = -1;
  media->video_stream_index = -1;

  media->pkt = av_packet_alloc();
  if (!media->pkt)
    return NULL;

  media->sws_ctx = NULL;

  return media;
}

int media_open_contexts(Media *media) {
  int ret;

  for (int i = 0; i < (int)media->fmt_ctx->nb_streams; i++) {
    AVStream *stream = media->fmt_ctx->streams[i];

    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
      printf("Could not find decoder for stream\n");
      return -1;
    }

    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
      printf("Could not allocate context");
      return ret;
    }

    if ((ret = avcodec_parameters_to_context(ctx, stream->codecpar)) < 0) {
      printf("Could not copy parameters from stream to context: %s\n",
             av_err2str(ret));
      return ret;
    }

    if ((ret = avcodec_open2(ctx, codec, NULL)) < 0) {
      printf("Could not open context: %s\n", av_err2str(ret));
      return ret;
    }

    // POSSIBLE ERROR HERE? LOOP INDEX MAY NOT CORRESPOND TO STREAM INDEX??
    media->ctxs[i] = ctx;
    media->streams[i] = stream;

    // This is a helper so we can easily find the most useful streams for now
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      media->video_stream_index = i;
      media->video_queue = fq_alloc();
      if (!media->video_queue)
        return -1;
    } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      media->audio_stream_index = i;
      media->audio_queue = fq_alloc();
      if (!media->audio_queue)
        return -1;
    }

    media->nb_streams++;
  }

  return 0;
}

AVCodecContext *media_get_audio_ctx(Media *media) {
  if (media->nb_streams <= 0 || media->audio_stream_index == -1) {
    return NULL;
  }

  return media->ctxs[media->audio_stream_index];
}

AVCodecContext *media_get_video_ctx(Media *media) {
  if (media->nb_streams <= 0 || media->video_stream_index == -1) {
    return NULL;
  }

  return media->ctxs[media->video_stream_index];
}

int media_init(Media *media, int dst_frame_w, int dst_frame_h,
               enum AVPixelFormat dst_frame_fmt) {

  media->dst_frame_w = dst_frame_w;
  media->dst_frame_h = dst_frame_h;
  media->dst_frame_fmt = dst_frame_fmt;

  int ret;

  // Setup the format context with the input info
  if ((ret = avformat_open_input(&media->fmt_ctx, media->filename, NULL,
                                 NULL)) < 0) {
    printf("Could not open source file wit libav %s\n", av_err2str(ret));
    return -1;
  }

  if ((ret = avformat_find_stream_info(media->fmt_ctx, NULL)) < 0) {
    printf("Could not find stream information: %s\n", av_err2str(ret));
    return -1;
  }

  av_dump_format(media->fmt_ctx, 0, media->filename, 0);

  // Open contexts
  ret = media_open_contexts(media);
  if (ret < 0) {
    printf("Could not open media contexts: %s\n", av_err2str(ret));
    return -1;
  }

  AVCodecContext *video_codec = media_get_video_ctx(media);
  if (video_codec) {
    media->sws_ctx = sws_getContext(
        video_codec->width, video_codec->height, video_codec->pix_fmt,
        dst_frame_w, dst_frame_h, dst_frame_fmt, SWS_BILINEAR, NULL, NULL, 0);

    if (!media->sws_ctx) {
      printf("Could not allocate sws context\n");
      return -1;
    }
  }

  return 0;
}

int media_read_frame(Media *media) {
  return av_read_frame(media->fmt_ctx, media->pkt);
}

int media_decode_frames(Media *media) {
  int ret;
  AVCodecContext *codec = media->pkt->stream_index == media->video_stream_index
                              ? media_get_video_ctx(media)
                              : media_get_audio_ctx(media);

  if ((ret = avcodec_send_packet(codec, media->pkt)) < 0) {
    return -1;
  }

  while (ret >= 0) {
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
      return -1;
    }

    ret = avcodec_receive_frame(codec, frame);
    if (ret < 0) {
      av_frame_free(&frame);
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
        return 0;
      }
      return -1;
    }

    if (media->pkt->stream_index == media->video_stream_index) {
      AVFrame *dst_frame = av_frame_alloc();
      uint8_t *dst_data[4];
      int dst_linesize[4];

      if (av_image_alloc(dst_data, dst_linesize, media->dst_frame_w,
                         media->dst_frame_h, media->dst_frame_fmt, 1) < 0) {
        printf("Failed to allocate image for scaling.\n");
        return -1;
      }

      if ((ret = sws_scale(media->sws_ctx, (const uint8_t *const *)frame->data,
                           frame->linesize, 0, frame->height, dst_data,
                           dst_linesize)) < 0) {
        printf("Failed to scale: %s.\n", av_err2str(ret));
        return -1;
      };

      // Copy scaled data to dst_frame
      av_image_copy(dst_frame->data, dst_frame->linesize,
                    (const uint8_t *const *)dst_data, dst_linesize,
                    dst_frame->format, dst_frame->width, dst_frame->height);

      printf("Enqueued video frame: %x\n", dst_frame);
      fq_enqueue(media->video_queue, dst_frame);

      // av_freep(&dst_data[0]);
      av_frame_free(&frame);
    } else {
      // Add audio frame to audio queue
      printf("Enqueued audio frame: %x\n", frame);
      fq_enqueue(media->audio_queue, frame);
    }
  }

  return 0;
}

void media_free(Media *media) {
  avformat_close_input(&media->fmt_ctx);

  for (int i = 0; i < media->nb_streams; i++) {
    avcodec_free_context(&media->ctxs[i]);
  }

  av_packet_free(&media->pkt);

  if (media->sws_ctx)
    sws_freeContext(media->sws_ctx);

  if (media->video_queue)
    fq_free(media->video_queue);

  if (media->audio_queue)
    fq_free(media->audio_queue);

  free(media);
}
