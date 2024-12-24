#include "media.h"

Media *media_alloc(int video_area_width, int video_area_height) {
  Media *media = (Media *)malloc(sizeof(Media));

  media->filename = (char *)malloc(sizeof(char) * MAX_FILEPATH_SIZE);
  media->fmt_ctx = NULL;

  media->video_stream_index = -1;
  media->audio_stream_index = -1;

  media->audio_codec = NULL;
  media->video_codec = NULL;

  media->pkt = av_packet_alloc();
  media->frame = av_frame_alloc();
  media->dst_frame = av_frame_alloc();

  media->video_area_width = video_area_width;
  media->video_area_height = video_area_height;

  media->sws_ctx = NULL;

  return media;
}

// This opens either the audio or the video codec for later use in decoding
AVCodecContext *media_open_context(Media *media, enum AVMediaType type) {
  int ret;

  const AVCodec *desired_codec;
  if ((ret = av_find_best_stream(media->fmt_ctx, type, -1, -1, &desired_codec,
                                 0)) < 0) {
    if (ret == AVERROR_STREAM_NOT_FOUND) {
      printf("Could not find the desired stream for type\n");
    }

    if (ret == AVERROR_DECODER_NOT_FOUND) {
      printf("Could not find the desired decoder for type\n");
    }

    printf("Could not find best stream: %s\n", av_err2str(ret));
    return NULL;
  }

  // Set the stream index. Might be useful later on
  if (type == AVMEDIA_TYPE_VIDEO) {
    media->video_stream_index = ret;
  } else {
    media->audio_stream_index = ret;
  }

  AVCodecContext *ctx = avcodec_alloc_context3(desired_codec);
  if (!ctx) {
    printf("Could not allocate context");
    return NULL;
  }

  if ((ret = avcodec_parameters_to_context(
           ctx, media->fmt_ctx->streams[ret]->codecpar)) < 0) {
    printf("Could not copy info to context: %s\n", av_err2str(ret));
    return NULL;
  }

  if ((ret = avcodec_open2(ctx, desired_codec, NULL)) < 0) {
    printf("Could not open context: %s\n", av_err2str(ret));
    return NULL;
  }

  return ctx;
}

int media_init(Media *media) {
  int ret;

  // Setup the format context with the input info
  if ((ret = avformat_open_input(&media->fmt_ctx, media->filename, NULL,
                                 NULL)) < 0) {
    printf("Could not open source file %s\n", av_err2str(ret));
    return -1;
  }

  if ((ret = avformat_find_stream_info(media->fmt_ctx, NULL)) < 0) {
    printf("Could not find stream information: %s\n", av_err2str(ret));
    return -1;
  }

  av_dump_format(media->fmt_ctx, 0, media->filename, 0);

  // Open contexts
  media->video_codec = media_open_context(media, AVMEDIA_TYPE_VIDEO);
  if (!media->video_codec) {
    return -1;
  }

  media->attached_pic =
      media->fmt_ctx->streams[media->video_stream_index]->attached_pic;

  media->audio_codec = media_open_context(media, AVMEDIA_TYPE_AUDIO);
  if (!media->audio_codec) {
    return -1;
  }

  // Create the conversion context
  media->dst_frame->width = media->video_area_width;
  media->dst_frame->height = media->video_area_height;
  media->dst_frame->format = AV_PIX_FMT_RGBA;

  media->sws_ctx =
      sws_getContext(media->video_codec->width, media->video_codec->height,
                     media->video_codec->pix_fmt, media->dst_frame->width,
                     media->dst_frame->height, media->dst_frame->format,
                     SWS_BILINEAR, NULL, NULL, 0);

  if (!media->sws_ctx) {
    return -1;
  }

  return 0;
}

int media_read_frame(Media *media) {
  if (av_read_frame(media->fmt_ctx, media->pkt) < 0) {
    return -1;
  }

  return 0;
}

int media_decode_frames(Media *media) {
  int ret;
  AVCodecContext *codec = media->pkt->stream_index == media->video_stream_index
                              ? media->video_codec
                              : media->audio_codec;

  if ((ret = avcodec_send_packet(codec, media->pkt)) < 0) {
    return -1;
  }

  while (ret >= 0) {
    ret = avcodec_receive_frame(codec, media->frame);
    if (ret < 0) {
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
        break;
      }

      return -1;
    }

    if (media->pkt->stream_index == media->video_stream_index) {
      // Add video frame to video queue
      if ((ret = sws_scale_frame(media->sws_ctx, media->dst_frame,
                                 media->frame)) < 0) {

        printf("Could not scale frame: %s\n", av_err2str(ret));
      };
    } else {
      // Add audio frame to audio queue
    }

    av_frame_unref(media->frame);
  }

  int index = media->pkt->stream_index == media->video_stream_index
                  ? media->video_stream_index
                  : media->audio_stream_index;
  av_packet_unref(media->pkt);
  return index;
}

void media_free(Media *media) {
  avformat_close_input(&media->fmt_ctx);

  sws_freeContext(media->sws_ctx);
  avcodec_free_context(&media->video_codec);
  avcodec_free_context(&media->audio_codec);

  av_packet_free(&media->pkt);
  av_frame_free(&media->frame);
  av_frame_free(&media->dst_frame);

  free(media);
}
