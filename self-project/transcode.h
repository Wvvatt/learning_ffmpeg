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
struct CodecLayer
{
    bool FillDecoder(AVStream *);
    bool FillEncoderCopyFrom(AVCodecParameters *);
    bool FillVideoEncoder(AVCodecContext *, const std::string &, AVRational);
    bool FillAudioEncoder(AVCodecContext *, const std::string &);
    bool OpenCodec();
    bool CodecExist() const;

    bool Decode(AVPacket *, AVFrame *);
    bool Encode(AVFrame *, AVPacket *);

    int index{-1};
    AVStream *stream{nullptr};
    const AVCodec *codec{nullptr};
    AVCodecContext *codec_ctx{nullptr};
};

struct PackageLayer
{
    bool FillDecoder(AVStream *);
    bool FillEncoderCopyFrom(const CodecLayer&);
    bool FillEncoderSetby(const CodecLayer&, const std::string &);

    bool OpenFileAndInit();
    void WirteTailAndClose();

    bool ReadPacket(AVPacket *);
    bool WritePacket(AVPacket *);

    std::string file_name;
    AVFormatContext *avfmt{nullptr};
    CodecLayer v_codec_layer_;
    CodecLayer a_codec_layer_;
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

    PackageLayer input_package_layer_;
    PackageLayer output_package_layer_;

    bool work_running_{false};
    std::thread work_thread_;
};