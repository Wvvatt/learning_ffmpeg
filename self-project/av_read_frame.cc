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
    fprintf( stderr, "LOG: ");
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}


char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define err2str(errnum) av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)

int main(int argc, char** argv)
{
    if(argc < 2){
        logging("please specify a file name:\n ./av_read_frame [filename]");
        return -1;
    }
    const char *filename = argv[1];
    AVFormatContext *in_fmtctx = avformat_alloc_context();
    // AVDictionary *opts = nullptr;
    // av_dict_set(&opts, "merge_pmt_versions", "1", 0);
    if(0 != avformat_open_input(&in_fmtctx, filename, nullptr, nullptr)){
        logging("avformat_open_input failed!");
        return -1;
    }
    logging("Format %s, duration %lld us", in_fmtctx->iformat->long_name, in_fmtctx->duration);
    if(0 != avformat_find_stream_info(in_fmtctx, NULL)){
        logging("avformat_find_stream_info failed!");
        return -1;
    }
    av_dump_format(in_fmtctx, 0, filename, 0);

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

    while(av_read_frame(in_fmtctx, packet) >= 0){
        logging("AVPacket->pts = %ld", packet->pts);
        av_packet_unref(packet);
    }

    if(packet) av_packet_free(&packet);
    if(frame) av_frame_free(&frame);
    if(in_fmtctx) avformat_close_input(&in_fmtctx);
}
