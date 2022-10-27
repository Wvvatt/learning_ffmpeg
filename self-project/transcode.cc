#include "transcode.h"

#define logging(fmt, ...) fprintf(stderr, "[%s %d]" fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);

static constexpr char usage_string[] = "usage:\n"
                                       "    ./transcode [-i input] {[codec options] -o output}\n"
                                       "-vcodec video_codec_type\n"
                                       "-acodec audio_codec_type\n"
                                       "-codec av_codec_type\tif output code type is the same as input codec type, please set -codec copy\n"
                                       "\n";

bool StreamContext::FillDecoder(AVStream *in_stream)
{
    index = in_stream->index;
    stream = in_stream;
    codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
    if (!codec)
    {
        logging("failed to find video decoder!");
        return false;
    }
    if(codec_ctx){
        avcodec_free_context(&codec_ctx);
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        logging("failed to alloc memory for codec context!");
        return false;
    }
    if (avcodec_parameters_to_context(codec_ctx, in_stream->codecpar) < 0)
    {
        logging("failed to fill codec context!");
        return false;
    }

    return true;
}

bool StreamContext::FillEncoderCopyFrom(AVCodecParameters *in_codecpar)
{
    avcodec_parameters_copy(stream->codecpar, in_codecpar);
    return true;
}

bool StreamContext::FillEncoderSetby(AVCodecContext *dec_ctx, const std::string& id)
{
    codec = avcodec_find_encoder_by_name(id.c_str());
    if(!codec) {
        logging("failed to find encoder codec");
        return false;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if(!codec_ctx){
        logging("failed to alloc context");
        return false;
    }
    // video
    codec_ctx->height = dec_ctx->height;
    codec_ctx->width = dec_ctx->width;
    codec_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
    if(codec_ctx->pix_fmt){
        codec_ctx->pix_fmt = dec_ctx->pix_fmt;
    }
    codec_ctx->bit_rate = 2 * 1000 * 1000;
    codec_ctx->rc_buffer_size = 4 * 1000 * 1000;
    codec_ctx->rc_max_rate = 2 * 1000 * 1000;
    codec_ctx->rc_min_rate = 2 * 1000 * 1000;
    // audio
    codec_ctx->sample_rate = dec_ctx->sample_rate;
    codec_ctx->sample_fmt = dec_ctx->sample_fmt;
    codec_ctx->bit_rate = dec_ctx->bit_rate;

    stream->time_base = codec_ctx->time_base = dec_ctx->time_base;

    avcodec_parameters_from_context(stream->codecpar, codec_ctx);

    return true;
}

bool StreamContext::OpenCodec()
{
    if (index != -1 && codec && codec_ctx && (avcodec_open2(codec_ctx, codec, NULL) >= 0))
    {
        return true;
    }
    else
    {
        logging("failed to open codec!");
        return false;
    }
}

bool StreamContext::Exist() const
{
    if (index != -1 && stream && codec && codec_ctx)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// 每种类型只记录第一个流
bool FormatContext::FillDecoder(AVStream *stream)
{
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && v_stream_ctx.index == -1)
    {
        return v_stream_ctx.FillDecoder(stream);
    }
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && a_stream_ctx.index == -1)
    {
        return a_stream_ctx.FillDecoder(stream);
    }
    logging("this input stream [%d : %s] is not the type we need!",
            stream->index, av_get_media_type_string(stream->codecpar->codec_type));
    return true;
}

// copy不需要AVCodecContext
bool FormatContext::FillEncoderCopyFrom(AVStream *in_stream)
{
    if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !v_stream_ctx.stream)
    {
        v_stream_ctx.stream = avformat_new_stream(avfmt, nullptr);
        v_stream_ctx.FillEncoderCopyFrom(in_stream->codecpar);
    }
    if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !a_stream_ctx.stream)
    {
        a_stream_ctx.stream = avformat_new_stream(avfmt, nullptr);
        a_stream_ctx.FillEncoderCopyFrom(in_stream->codecpar);
    }
    logging("this input stream [%d : %s] is not the type we need!",
            in_stream->index, av_get_media_type_string(in_stream->codecpar->codec_type));
    return true;
}

bool FormatContext::FillEncoderSetby(const StreamContext &in_stream_ctx, const std::string& id)
{
    if (in_stream_ctx.stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !v_stream_ctx.stream)
    {
        v_stream_ctx.stream = avformat_new_stream(avfmt, nullptr);
        v_stream_ctx.FillEncoderSetby(in_stream_ctx.codec_ctx, id);
    }
    if (in_stream_ctx.stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !a_stream_ctx.stream)
    {
        a_stream_ctx.stream = avformat_new_stream(avfmt, nullptr);
        a_stream_ctx.FillEncoderSetby(in_stream_ctx.codec_ctx, id);
    }
    logging("this input stream [%d : %s] is not the type we need!",
            in_stream_ctx.index, av_get_media_type_string(in_stream_ctx.stream->codecpar->codec_type));
    return true;
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
    if(v_encode_type_.empty() || v_encode_type_ == "copy"){
        v_copy_ = true;
    }
    if(a_encode_type_.empty() || a_encode_type_ == "copy"){
        a_copy_ = true;
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
    if(!OpenOutput())
    {
        logging("failed to open output!");
        return false;
    }
    return true;
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
    for (uint32_t i = 0; i < input_fmtctx_.avfmt->nb_streams; i++)
    {
        AVStream *stream = input_fmtctx_.avfmt->streams[i];
        input_fmtctx_.FillDecoder(stream);
    }
    if (!input_fmtctx_.v_stream_ctx.OpenCodec() || !input_fmtctx_.a_stream_ctx.OpenCodec())
    {
        return false;
    }

    return true;
}

bool Transcoder::OpenOutput()
{
    avformat_alloc_output_context2(&output_fmtctx_.avfmt, nullptr, nullptr, output_fmtctx_.file_name.c_str());
    if (!output_fmtctx_.avfmt)
    {
        logging("failed to alloce output format context!");
        return false;
    }
    if (input_fmtctx_.v_stream_ctx.Exist())
    {
        if (v_encode_type_.empty() || v_encode_type_ == "copy")
        {
            output_fmtctx_.FillEncoderCopyFrom(input_fmtctx_.v_stream_ctx.stream);
        }
        else
        {   
            output_fmtctx_.FillEncoderSetby(input_fmtctx_.v_stream_ctx, v_encode_type_);
        }
    }
    if (input_fmtctx_.a_stream_ctx.Exist())
    {
        if (a_encode_type_.empty() || a_encode_type_ == "copy")
        {
            output_fmtctx_.FillEncoderCopyFrom(input_fmtctx_.a_stream_ctx.stream);
        }
        else
        {
            output_fmtctx_.FillEncoderSetby(input_fmtctx_.a_stream_ctx, a_encode_type_);
        }
    }

    if(!v_copy_ && !output_fmtctx_.v_stream_ctx.OpenCodec()){
        logging("failed to open video encoder!");
        return false;
    }
    if(!a_copy_ && !output_fmtctx_.a_stream_ctx.OpenCodec()){
        logging("failed to open audio encoder!");
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    Transcoder transcoder;
    if (!transcoder.ParseParam(argc, argv))
    {
        logging("param is not valid!");
        return -1;
    }
    if(!transcoder.Open()){
        return -1;
    }
}