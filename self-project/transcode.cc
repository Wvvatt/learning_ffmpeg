#include "transcode.h"

#define logging(fmt, ...) fprintf(stderr, fmt"\n", ##__VA_ARGS__);

static constexpr char usage_string[] = "usage:\n"
                                       "    ./transcode [-i input] {[codec options] -o output}\n"
                                       "-vcodec video_codec_type\n"
                                       "-acodec audio_codec_type\n"
                                       "-codec av_codec_type\tif output code type is the same as input codec type, please set -codec copy\n"
                                       "\n";

bool StreamContext::Fill(AVStream* stream)
{
    index = stream->index;
    stream = stream;
    codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if(!codec){
        logging("failed to find video decoder!");
        return false;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if(!codec_ctx){
        logging("failed to alloc memory for codec context!");
        return false;
    }
    if(avcodec_parameters_to_context(codec_ctx, stream->codecpar) < 0){
        logging("failed to fill codec context!");
        return false;
    }
}

bool StreamContext::OpenDecoder()
{
    if(index != -1 && codec && codec_ctx && (avcodec_open2(codec_ctx, codec, NULL) >= 0)){
        return true;
    }
    else{
        logging("failed to open decoder!");
        return false;
    }
}

bool Transcoder::ParseParam(int argc, char **argv)
{
    int lopt;
    option long_opts[] = {
        {"vcodec", required_argument, &lopt, 1},
        {"acodec", required_argument, &lopt, 2},
        {0, 0, 0, 0}};
    int ret;
    int opt_index;
    const char *optstring = "i:o:h";

    std::string &in_filename = input_fmtctx_.file_name;
    std::string &out_filename = output_fmtctx_.file_name;
    v_encode_type_.clear();
    a_encode_type_.clear();

    while ((ret = getopt_long(argc, argv, optstring, long_opts, &opt_index)) != -1)
    {
        if (ret == 'h')
        {
            printf(usage_string);
            break;
        }
        switch (ret)
        {
        case 'i':
            in_filename = optarg;
            break;
        case 'o':
            out_filename = optarg;
            break;
        case 0:
            switch (lopt)
            {
            case 1:
                v_encode_type_ = optarg;
                break;
            case 2:
                a_encode_type_ = optarg;
                break;
            }
            break;

        case '?':
        default:
            break;
        }
    }
    if (in_filename.empty() || out_filename.empty())
    {
        logging("input and output is neccessary!");
        return false;
    }
    return true;
}

bool Transcoder::Open()
{
    if (!OpenInput())
    {
        logging("failed to open input!");
        return false;
    }
}

bool Transcoder::OpenInput()
{
    // open media
    input_fmtctx_.avfmt = avformat_alloc_context();
    if (avformat_open_input(&input_fmtctx_.avfmt, input_fmtctx_.file_name.c_str(), 0, 0) < 0)
    {
        return false;
    }
    if (avformat_find_stream_info(input_fmtctx_.avfmt, 0) < 0)
    {
        return false;
    }
    av_dump_format(input_fmtctx_.avfmt, 0, input_fmtctx_.file_name.c_str(), 0);

    // set decoder
    for (int i = 0; i < input_fmtctx_.avfmt->nb_streams; i++)
    {
        AVStream * stream = input_fmtctx_.avfmt->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if(!input_fmtctx_.video_stream.Fill(stream)){
                return false;
            }
        }
        else if(stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            if(!input_fmtctx_.audio_stream.Fill(stream)){
                return false;
            }
        }
        else{
            logging("%s is not stream type that we need", av_get_media_type_string(stream->codecpar->codec_type));
            continue;
        }
    }
    if(!input_fmtctx_.video_stream.OpenDecoder() || !input_fmtctx_.audio_stream.OpenDecoder()){
        return false;
    }

    return true;
}

bool Transcoder::OpenOutput()
{
    avformat_alloc_output_context2(&output_fmtctx_.avfmt, nullptr, nullptr, output_fmtctx_.file_name.c_str());
    if(!output_fmtctx_.avfmt){
        logging("failed to alloce output format context!");
        return false;
    }
}

int main(int argc, char **argv)
{
    Transcoder transcoder;
    if (!transcoder.ParseParam(argc, argv))
    {
        logging("param is not valid!");
        return -1;
    }
}