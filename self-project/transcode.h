#include <stdio.h>
#include <unistd.h>
#include <string>
#include <getopt.h>
#ifdef __cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/frame.h"
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>
#ifdef __cplusplus
}
#endif

/*
    in --> demuxer --av_packet--> decoder --av_frame--> coder --av_packet--> muxer --> out
*/

// 把codec部分从avformat中提取出来自己掌控
struct StreamContext
{
    bool FillDecoder(AVStream *);
    bool FillEncoderCopyFrom(AVCodecParameters *);
    bool FillVideoEncoder(AVCodecContext *, const std::string &, AVRational);
    bool FillAudioEncoder(AVCodecContext *, const std::string &);
    bool OpenCodec();
    bool Exist() const;

    int index{-1};
    AVStream *stream{nullptr};
    const AVCodec *codec{nullptr};
    AVCodecContext *codec_ctx{nullptr};
};

struct FormatContext
{
    bool FillDecoder(AVStream *);
    bool FillEncoderCopyFrom(const StreamContext&);
    bool FillEncoderSetby(const StreamContext&, const std::string &);

    bool OpenFileInit();
    
    std::string file_name;
    AVFormatContext *avfmt{nullptr};
    StreamContext v_stream_ctx;
    StreamContext a_stream_ctx;
};

class Transcoder
{
public:
    bool ParseParam(int argc, char **argv);
    bool Open();

private:
    bool OpenInput();
    bool OpenOutput();

private:
    std::string v_encode_type_;
    std::string a_encode_type_;
    bool v_copy_{false};
    bool a_copy_{false};

    FormatContext input_fmtctx_;
    FormatContext output_fmtctx_;
};