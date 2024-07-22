#include <iostream>
#include <unistd.h>
#include <memory>
#ifdef __cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavcodec/bsf.h"
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#include <libavutil/opt.h>
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

    int video_stream_index = av_find_best_stream(in_fmtctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_index < 0) {
        logging("can not find video stream\n");
        return -1;
    }

    AVBSFContext *bsf_ctx = NULL;
    if(0 != av_bsf_alloc(av_bsf_get_by_name("h264_mp4toannexb"), &bsf_ctx)){
        logging("can not alloc bsf");
        return -1;
    }
    if(0 != avcodec_parameters_copy(bsf_ctx->par_in, in_fmtctx->streams[video_stream_index]->codecpar)){
        logging("can not copy codec parameters to bsf");
        return -1;
    }
    if(0 != av_bsf_init(bsf_ctx)){
        logging("bsf init failed");
        return -1;
    }

    AVPacket pkt, filtered_pkt;
    while (av_read_frame(in_fmtctx, &pkt) >= 0) {
        if (pkt.stream_index == video_stream_index) {
            logging("bsf send packet: %d", pkt.size);
            if (av_bsf_send_packet(bsf_ctx, &pkt) != 0) {
                logging("send packet to bsf failed");
                return -1;
            }
            if (av_bsf_receive_packet(bsf_ctx, &filtered_pkt) == 0) {
                logging("bsf receive packet: %d", filtered_pkt.size);
                av_packet_unref(&filtered_pkt);
                break;
            }
        }
        av_packet_unref(&pkt);
    }

    av_bsf_free(&bsf_ctx);
    avformat_close_input(&in_fmtctx);
}
