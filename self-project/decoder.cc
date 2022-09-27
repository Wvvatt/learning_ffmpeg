#include <iostream>
#include <chrono>
#include <unistd.h>
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
using namespace std::chrono;
milliseconds get_now_time_ms()
{
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch());
}

int main(int argc, char** argv)
{
    AVFormatContext* ifmt_ctx = nullptr;

    const char* file_name = argv[1];
    if(avformat_open_input(&ifmt_ctx, file_name, 0, 0) < 0){
        printf("could not open input file");
        return -1;
    }
    if(avformat_find_stream_info(ifmt_ctx, 0) < 0){
        printf("could not find stream info");
        return -1;
    }
    av_dump_format(ifmt_ctx, 0, file_name, 0);

    AVStream* v_stream = ifmt_ctx->streams[0];
    const AVCodec* codec = avcodec_find_decoder(v_stream->codecpar->codec_id);
    AVCodecContext* v_codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(v_codec_ctx, v_stream->codecpar);
    v_codec_ctx->thread_type = FF_THREAD_FRAME;

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "threads", "8", 0);
    avcodec_open2(v_codec_ctx, codec, &opts);
    av_dict_free(&opts);
    //printf("width=%d, height=%d\n", v_codec_ctx->width, v_codec_ctx->height);

    AVPacket pkt;
    AVFrame* av_frame = av_frame_alloc();
    while(1){
        auto ret = avcodec_receive_frame(v_codec_ctx, av_frame);
        if(ret == AVERROR_EOF){
            avcodec_flush_buffers(v_codec_ctx);
            break;
        }
        if(av_frame){
            printf("[after] pts=%ld, time=%ld\n", av_frame->pts, get_now_time_ms().count());
        }

        if(av_read_frame(ifmt_ctx, &pkt) < 0){
            break;
        }
        usleep(40000);

        if(pkt.stream_index == 0){
            printf("[before] pts=%ld, dts=%ld, time=%ld\n", pkt.pts, pkt.dts, get_now_time_ms().count());
            avcodec_send_packet(v_codec_ctx, &pkt);
        }
    }
    av_frame_free(&av_frame);
}
