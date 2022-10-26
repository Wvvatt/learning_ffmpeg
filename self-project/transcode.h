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
#ifdef __cplusplus
}
#endif

/*
    in --> demuxer --av_packet--> decoder --av_frame--> coder --av_packet--> muxer --> out
*/

struct StreamContext
{
    bool Fill(AVStream *);
    bool OpenDecoder();
    int index{-1};
    AVStream *stream{nullptr};
    const AVCodec *codec{nullptr};
    AVCodecContext *codec_ctx{nullptr};
};

struct FormatContext
{
    std::string file_name;
    AVFormatContext *avfmt{nullptr};
    StreamContext video_stream;
    StreamContext audio_stream;
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

    FormatContext input_fmtctx_;
    FormatContext output_fmtctx_;
};