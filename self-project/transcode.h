#include <stdio.h>
#include <unistd.h>
#include <string>
#include <getopt.h>
#include <thread>

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
struct CodecContext
{
    bool FillDecoder(AVStream *);
    bool FillEncoderCopyFrom(AVCodecParameters *);
    bool FillVideoEncoder(AVCodecContext *, const std::string &, AVRational);
    bool FillAudioEncoder(AVCodecContext *, const std::string &);
    bool OpenCodec();
    bool Exist() const;

    bool Decode(AVPacket *, AVFrame *);
    bool Encode(AVFrame *, AVPacket *);

    int index{-1};
    AVStream *stream{nullptr};
    const AVCodec *codec{nullptr};
    AVCodecContext *codec_ctx{nullptr};
};

struct PackageContext
{
    bool FillDecoder(AVStream *);
    bool FillEncoderCopyFrom(const CodecContext&);
    bool FillEncoderSetby(const CodecContext&, const std::string &);

    bool OpenFileAndInit();
    void WirteTailAndClose();

    bool ReadPacket(AVPacket *);
    bool WritePacket(AVPacket *);

    std::string file_name;
    AVFormatContext *avfmt{nullptr};
    CodecContext v_stream_ctx;
    CodecContext a_stream_ctx;
};

class Transcoder
{
public:
    bool ParseParam(int argc, char **argv);
    bool Open();
    void Close();
    void Start();
    void Stop();

private:
    void Work();
    bool OpenInput();
    bool OpenOutput();

private:
    std::string v_encode_type_;
    std::string a_encode_type_;
    bool v_copy_{false};
    bool a_copy_{false};

    PackageContext input_fmtctx_;
    PackageContext output_fmtctx_;

    bool work_running_{false};
    std::thread work_thread_;
};