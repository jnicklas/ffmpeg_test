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

int main(int argc, char **argv)
{
  int linesize[4];
  int width = 320, height = 240;
  enum AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
  const char *filename = "test.mpg";
  FILE *file;
  int bufsize, i, res, got_output;
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
  frame->format = c->pix_fmt;
  frame->width  = c->width;
  frame->height = c->height;

  bufsize = av_image_alloc(frame->data, linesize, width, height, pix_fmt, 1);
  check(bufsize >= 0, "failed to allocate memory for video buffer");

  res = ff_load_image(frame->data, linesize, &width, &height, &pix_fmt, "source/img0.jpg", NULL);
  check(res >= 0, "failed to load image");

  log_info("generating frames");
  for (i = 0; i < 50; i++) {
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    /* generate synthetic video */
    frame->pts = i;

    log_info("encoding it");
    res = avcodec_encode_video2(c, &pkt, frame, &got_output);
    check(res >= 0, "Error encoding frame");

    if (got_output) {
      log_info("Write frame %3d (size=%5d)", i, pkt.size);
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
  return 0;

error:
  if (file)
    fclose(file);
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
