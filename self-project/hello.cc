#include <iostream>
#include <unistd.h>
#include <memory>
#ifdef __cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/frame.h"
#ifdef __cplusplus
}
#endif

static void logging(const char *fmt, ...)
{
    va_list args;
    fprintf( stderr, "LOG: [%s:%d] ", __FILE__,  __LINE__);
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}

static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame);
static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename);

int main(int argc, char** argv)
{
    if(argc < 2){
        logging("please specify a file name:\n ./hello [filename]");
        return -1;
    }
    const char *filename = argv[1];
    AVFormatContext *av_fmt_ctx = avformat_alloc_context();
    if(0 != avformat_open_input(&av_fmt_ctx, filename, NULL, NULL)){
        logging("avformat_open_input failed!");
        return -1;
    }
    logging("Format %s, duration %lld us", av_fmt_ctx->iformat->long_name, av_fmt_ctx->duration);
    if(0 != avformat_find_stream_info(av_fmt_ctx, NULL)){
        logging("avformat_find_stream_info failed!");
        return -1;
    }
    const AVCodecParameters *video_codec_par = nullptr;
    const AVCodec *video_codec = nullptr;
    int video_index = -1;
    for (int i = 0; i < av_fmt_ctx->nb_streams; i++)
    {
        AVCodecParameters *local_codec_par = av_fmt_ctx->streams[i]->codecpar;
        const AVCodec *local_codec = avcodec_find_decoder(local_codec_par->codec_id);
        if(local_codec == nullptr){
            logging("unsupported codec_id!");
            continue;
        }
        if(local_codec_par->codec_type == AVMEDIA_TYPE_VIDEO){
            if(-1 == video_index){
                video_index = i;
                video_codec_par = local_codec_par;
                video_codec = local_codec;
            }
            logging("Video Codec: resolution %d x %d", local_codec_par->width, local_codec_par->height);
        }
        else if(local_codec_par->codec_type == AVMEDIA_TYPE_AUDIO){
            logging("Audio Codec: %d channels, sample rate %d", local_codec_par->channels, local_codec_par->sample_rate);
        }
        logging("\tCodec %s ID %d bit_rate %lld", local_codec->long_name, local_codec->id, local_codec_par->bit_rate);
    }
    if(video_index == -1){
        logging("cannot find video stream in %s", filename);
        return -1;
    }
    
    AVCodecContext *video_codec_ctx = avcodec_alloc_context3(video_codec);
    if(!video_codec_ctx){
        logging("alloc context failed!");
        return -1;
    }
    if(avcodec_parameters_to_context(video_codec_ctx, video_codec_par) < 0){
        logging("copy param to context failed!");
        return -1;
    }
    if(avcodec_open2(video_codec_ctx, video_codec, nullptr) < 0){
        logging("avcodec open failed!");
        return -1;
    }
    AVFrame *frame = av_frame_alloc();
    if(!frame){
        logging("av frame alloc failed!");
        return -1;
    }
    AVPacket *packet = av_packet_alloc();
    if(!packet){
        logging("av packet alloc failed!");
        return -1;
    }

    int ret = 0;
    int how_many_packets_to_process = 8;
    while(av_read_frame(av_fmt_ctx, packet) >= 0){
        if(packet->stream_index == video_index){
            logging("AVPacket->pts = %ld", packet->pts);
            ret = decode_packet(packet, video_codec_ctx, frame);
            if(ret < 0){
                break;
            }
            if(--how_many_packets_to_process <= 0) break;
        }
        av_packet_unref(packet);
    }
}

static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame)
{
  int response = avcodec_send_packet(pCodecContext, pPacket);
  if (response < 0) {
    logging("Error while sending a packet to the decoder: %s", av_err2str(response));
    return response;
  }
  while (response >= 0)
  {
    // Return decoded output data (into a frame) from a decoder
    response = avcodec_receive_frame(pCodecContext, pFrame);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      break;
    } else if (response < 0) {
      logging("Error while receiving a frame from the decoder: %s", av_err2str(response));
      return response;
    }

    if (response >= 0) {
      logging(
          "Frame %d (type=%c, size=%d bytes, format=%d) pts %d key_frame %d [DTS %d]",
          pCodecContext->frame_number,
          av_get_picture_type_char(pFrame->pict_type),
          pFrame->pkt_size,
          pFrame->format,
          pFrame->pts,
          pFrame->key_frame,
          pFrame->coded_picture_number
      );

      char frame_filename[1024];
      snprintf(frame_filename, sizeof(frame_filename), "%s-%d.pgm", "frame", pCodecContext->frame_number);
      // Check if the frame is a planar YUV 4:2:0, 12bpp
      // That is the format of the provided .mp4 file
      // RGB formats will definitely not give a gray image
      // Other YUV image may do so, but untested, so give a warning
      if (pFrame->format != AV_PIX_FMT_YUV420P)
      {
        logging("Warning: the generated file may not be a grayscale image, but could e.g. be just the R component if the video format is RGB");
      }
      // save a grayscale frame into a .pgm file
      save_gray_frame(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, frame_filename);
    }
  }
  return 0;
}

static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
{
    FILE *f;
    int i;
    f = fopen(filename,"w");
    // writing the minimal required header for a pgm file format
    // portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    // writing line by line
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}