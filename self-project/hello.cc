#include <iostream>
#include <chrono>
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

int main(int argc, char** argv)
{
    auto filename = argv[1];
    AVFormatContext *av_fmt_ctx = avformat_alloc_context();
    avformat_open_input(&av_fmt_ctx, filename, NULL, NULL);
    printf("Format %s, duration %lld us", av_fmt_ctx->iformat->long_name, av_fmt_ctx->duration);
    avformat_find_stream_info(av_fmt_ctx, NULL);
    for (int i = 0; i < av_fmt_ctx->nb_streams; i++)
    {
        AVCodecParameters *this_codec_par = av_fmt_ctx->streams[i]->codecpar;
        const AVCodec *this_codec = avcodec_find_decoder(this_codec_par->codec_id);
    }
}
