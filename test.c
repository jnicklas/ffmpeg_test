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

int main(int argc, char **argv)
{
  static struct SwsContext *sws_ctx;
  uint8_t *image_data[4];
  int linesize[4];
  int source_width, source_height;
  int width = 320, height = 240;
  enum AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
  enum AVPixelFormat source_fmt;
  const char *filename = "test.mpg";
  FILE *file;
  int i, res, got_output;
  AVCodec *codec;
  AVCodecContext *c= NULL;
  AVFrame *frame;
  AVPacket pkt;
  uint8_t endcode[] = { 0, 0, 1, 0xb7 };

  avcodec_register_all();

  file = fopen(filename, "wb");
  check(file != NULL, "could not open destination file %s", filename);

  codec = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);
  check(codec, "unable to find codec");

  c = avcodec_alloc_context3(codec);
  check(c, "unable to allocate codec");

  c->bit_rate = 400000;
  c->width = width;
  c->height = height;
  c->time_base= (AVRational){1,25};
  c->gop_size = 10;
  c->max_b_frames=1;
  c->pix_fmt = pix_fmt;

  res = avcodec_open2(c, codec, NULL);
  check(res >= 0, "could not open codec");

  frame = avcodec_alloc_frame();
  check(frame, "unable to allocate frame");

  res = ff_load_image(image_data, linesize, &source_width, &source_height, &source_fmt, "source/img0.jpg", NULL);
  check(res >= 0, "failed to load image");

  res = av_image_alloc(frame->data, frame->linesize, c->width, c->height, c->pix_fmt, 1);
  check(res >= 0, "failed to allocate memory for video frame");

  frame->height = c->height;
  frame->width = c->width;
  frame->format = c->pix_fmt;

  if (source_fmt != c->pix_fmt) {
    sws_ctx = sws_getContext(source_width, source_height, source_fmt,
        c->width, c->height, c->pix_fmt,
        sws_flags, NULL, NULL, NULL);
    check(sws_ctx, "unable to initialize scaling context");

    log_info("converting between pixel formats %d and %d", source_fmt, c->pix_fmt);
    sws_scale(sws_ctx,
        (const uint8_t * const *)image_data, linesize,
        0, c->height, frame->data, frame->linesize);
  }

  log_info("generating frames");
  for (i = 0; i < 50; i++) {
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    /* generate synthetic video */
    frame->pts = i;

    res = avcodec_encode_video2(c, &pkt, frame, &got_output);
    check(res >= 0, "Error encoding frame");

    if (got_output) {
      fwrite(pkt.data, 1, pkt.size, file);
      av_free_packet(&pkt);
    }
  }

  log_info("get delayed frames");

  /* get the delayed frames */
  for (got_output = 1; got_output; i++) {
    log_info("delayed frame %d", i);
    res = avcodec_encode_video2(c, &pkt, NULL, &got_output);
    check(res >= 0, "Error encoding frame");

    if (got_output) {
      fwrite(pkt.data, 1, pkt.size, file);
      av_free_packet(&pkt);
    }
  }

  log_info("done");


  fwrite(endcode, 1, sizeof(endcode), file);
  fclose(file);

  avcodec_close(c);
  av_free(c);
  av_freep(&frame->data[0]);
  av_free(frame);
  av_freep(&image_data);
  return 0;

error:
  if (file)
    fclose(file);
  av_freep(&image_data);
  if (c) {
    avcodec_close(c);
    av_free(c);
  }
  if (frame) {
    av_freep(&frame->data[0]);
    av_free(frame);
  }
  return -1;
}
