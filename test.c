#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
#include <libswscale/swscale.h>
#include <stdio.h>
#include "dbg.h"

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include "lavfutils.h"

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

static int sws_flags = SWS_BICUBIC;

int load_image_into_frame(AVFrame *frame, const char *filename)
{
  int retval = -1, res;
  static struct SwsContext *sws_ctx;
  uint8_t *image_data[4];
  int linesize[4];
  int source_width, source_height;
  enum AVPixelFormat source_fmt;

  res = ff_load_image(image_data, linesize, &source_width, &source_height, &source_fmt, filename, NULL);
  check(res >= 0, "failed to load image");

  if (source_fmt != frame->format) {
    sws_ctx = sws_getContext(source_width, source_height, source_fmt,
        frame->width, frame->height, frame->format,
        sws_flags, NULL, NULL, NULL);
    check(sws_ctx, "unable to initialize scaling context");

    sws_scale(sws_ctx,
        (const uint8_t * const *)image_data, linesize,
        0, frame->height, frame->data, frame->linesize);
  }

  retval = 0;
error:
  av_freep(image_data);
  av_free(sws_ctx);
  return retval;
}

int write_frame_to_file(FILE *file, AVFrame *frame, AVCodecContext *codec_context, AVPacket *pkt) {
  int res, got_output;
  av_init_packet(pkt);
  pkt->data = NULL;
  pkt->size = 0;

  /* generate synthetic video */
  frame->pts += 1;

  res = avcodec_encode_video2(codec_context, pkt, frame, &got_output);
  check(res >= 0, "Error encoding frame");

  if (got_output) {
    fwrite(pkt->data, 1, pkt->size, file);
    av_free_packet(pkt);
  }
  return 0;
error:
  return -1;
}

int write_image_to_file(FILE *file, const char *filename, int count, AVFrame *frame, AVCodecContext *codec_context, AVPacket *pkt) {
  int res, i;
  res = load_image_into_frame(frame, filename);
  check(res >= 0, "failed to load image into frame");

  for (i = 0; i < count; i++) {
    res = write_frame_to_file(file, frame, codec_context, pkt);
    check(res >= 0, "unable to write frame to file");
  }

  return 0;
error:
  return -1;
}

int write_delayed_frames_to_file(FILE *file, AVFrame *frame, AVCodecContext *codec_context, AVPacket *pkt) {
  int res, got_output;

  for (got_output = 1; got_output;) {
    res = avcodec_encode_video2(codec_context, pkt, NULL, &got_output);
    check(res >= 0, "Error encoding frame");

    if (got_output) {
      fwrite(pkt->data, 1, pkt->size, file);
      av_free_packet(pkt);
    }
  }

  return 0;
error:
  return -1;
}

AVCodecContext *get_codec_context(width, height, fps)
{
  int res;
  avcodec_register_all();

  AVCodec *codec;
  AVCodecContext *codec_context = NULL;

  codec = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);
  check(codec, "unable to find codec");

  codec_context = avcodec_alloc_context3(codec);
  check(codec_context, "unable to allocate codec");

  codec_context->bit_rate = 400000;
  codec_context->width = width;
  codec_context->height = height;
  codec_context->time_base= (AVRational){1,fps};
  codec_context->gop_size = 10;
  codec_context->max_b_frames=1;
  codec_context->pix_fmt = AV_PIX_FMT_YUV420P;

  res = avcodec_open2(codec_context, codec, NULL);
  check(res >= 0, "could not open codec");

  return codec_context;
error:
  return NULL;
}

AVFrame *get_av_frame(AVCodecContext *codec_context) {
  int res;
  AVFrame *frame;

  frame = avcodec_alloc_frame();
  check(frame != NULL, "unable to allocate frame");
  frame->height = codec_context->height;
  frame->width = codec_context->width;
  frame->format = codec_context->pix_fmt;
  frame->pts = 0;

  res = av_image_alloc(frame->data, frame->linesize, frame->width, frame->height, frame->format, 1);
  check(res >= 0, "failed to allocate memory for video frame");

  return frame;
error:
  return NULL;
}

int main(int argc, char **argv)
{
  const char *filename = "test.mpg";
  FILE *file;
  int res, retval=-1;
  AVCodecContext *codec_context= NULL;
  AVFrame *frame;
  AVPacket pkt;
  uint8_t endcode[] = { 0, 0, 1, 0xb7 };

  codec_context = get_codec_context(320, 240, 25);
  check(codec_context != NULL, "unable to obtain encoding context");

  file = fopen(filename, "wb");
  check(file != NULL, "could not open destination file %s", filename);

  frame = get_av_frame(codec_context);
  check(frame != NULL, "unable to allocate frame");

  res = write_image_to_file(file, "source/img0.jpg", 25, frame, codec_context, &pkt);
  res = write_image_to_file(file, "source/img1.jpg", 50, frame, codec_context, &pkt);
  res = write_image_to_file(file, "source/img2.jpg", 10, frame, codec_context, &pkt);
  res = write_image_to_file(file, "source/img3.jpg", 10, frame, codec_context, &pkt);
  check(res >= 0, "failed to write image to file");


  res = write_delayed_frames_to_file(file, frame, codec_context, &pkt);
  check(res >= 0, "failed to write delayed frames");

  fwrite(endcode, 1, sizeof(endcode), file);

  retval = 0;
error:
  if (file)
    fclose(file);
  if (codec_context) {
    avcodec_close(codec_context);
    av_free(codec_context);
  }
  if (frame) {
    av_freep(&frame->data[0]);
    av_free(frame);
  }
  return retval;
}
