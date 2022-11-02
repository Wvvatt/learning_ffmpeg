#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <execinfo.h>
#include <thread>
#include <semaphore.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include "libavutil/rational.h"
#ifdef __cplusplus
}
#endif
#include "transcode.h"

#define logging(fmt, ...) fprintf(stderr, "[%s %d]" fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);

static constexpr char usage_string[] = "usage:\n"
                                       "    ./transcode [-i input] {[codec options] -o output}\n"
                                       "-vcodec video_codec_type\n"
                                       "-acodec audio_codec_type\n"
                                       "-codec av_codec_type\tif output code type is the same as input codec type, please set -codec copy\n"
                                       "\n";

char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define err2str(errnum) av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)

static sem_t sem;
static void int_handler(int signum)
{
    signal(SIGINT, SIG_DFL);
    sem_post(&sem);
}

static void segv_handler(int signum)
{
    void *buffer[256];
    signal(signum, SIG_DFL);
    int fd = open("backtrace", O_RDWR | O_CREAT | O_TRUNC);
    if (fd > 0)
    {
        int size = backtrace(buffer, 256);
        backtrace_symbols_fd(buffer, size, fd);
        close(fd);
    }

    exit(1);
}

bool CodecLayer::FillDecoder(AVStream *in_stream)
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

bool CodecLayer::FillEncoderCopyFrom(AVCodecParameters *in_codecpar)
{
    if(avcodec_parameters_copy(stream->codecpar, in_codecpar) < 0)
    {
        return false;
    }
    return true;
}

bool CodecLayer::FillVideoEncoder(AVCodecContext *dec_ctx, const std::string& id, AVRational frame_rate)
{
    codec = avcodec_find_encoder_by_name(id.c_str());
    if(!codec) {
        logging("failed to find video encoder codec");
        return false;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if(!codec_ctx){
        logging("failed to alloc context");
        return false;
    }

    codec_ctx->height = dec_ctx->height;
    codec_ctx->width = dec_ctx->width;
    codec_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
    if(codec->pix_fmts){
        codec_ctx->pix_fmt = codec->pix_fmts[0];
    }
    else{
        codec_ctx->pix_fmt = dec_ctx->pix_fmt;
    }

    codec_ctx->bit_rate = 2 * 1000 * 1000;
    codec_ctx->rc_buffer_size = 4 * 1000 * 1000;
    codec_ctx->rc_max_rate = 2 * 1000 * 1000;
    codec_ctx->rc_min_rate = 2.5 * 1000 * 1000;

    stream->time_base = codec_ctx->time_base = av_inv_q(frame_rate);

    avcodec_parameters_from_context(stream->codecpar, codec_ctx);

    return true;
}


bool CodecLayer::FillAudioEncoder(AVCodecContext *dec_ctx, const std::string& id)
{
    codec = avcodec_find_encoder_by_name(id.c_str());
    if(!codec) {
        logging("failed to find audio encoder codec");
        return false;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if(!codec_ctx){
        logging("failed to alloc context");
        return false;
    }

    codec_ctx->channels = 2;
    codec_ctx->channel_layout = av_get_default_channel_layout(2);
    codec_ctx->sample_rate = dec_ctx->sample_rate;
    codec_ctx->sample_fmt = codec->sample_fmts[0];
    codec_ctx->bit_rate = dec_ctx->bit_rate;

    codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    stream->time_base = codec_ctx->time_base = (AVRational){1, dec_ctx->sample_rate};

    avcodec_parameters_from_context(stream->codecpar, codec_ctx);

    return true;
}

bool CodecLayer::OpenCodec()
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

bool CodecLayer::CodecExist() const
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

bool CodecLayer::Decode(AVPacket *in_pkt, AVFrame *out_frame)
{
    if(!in_pkt || !out_frame){
        return false;
    }
    int res = avcodec_send_packet(codec_ctx, in_pkt);
    if(res < 0){
        logging("error while sending packet to decoder: [%s]", err2str(res));
        return false;
    }
    res = avcodec_receive_frame(codec_ctx, out_frame);
    if(res >= 0){
        return true;
    }
    else if(res == AVERROR(EAGAIN) || res == AVERROR_EOF){
        logging("cannot receive frame from decoder this time, try again");
        return false;
    }
    else{ // <0
        logging("error while receive frame from decoder: [%s]", err2str(res));
        return false;
    }
}

bool CodecLayer::Encode(AVFrame *in_frame, AVPacket *out_pkt)
{
    if(!in_frame || !out_pkt){
        return false;
    }
    int res = avcodec_send_frame(codec_ctx, in_frame);
    if(res < 0){
        logging("error while sending frame to encoder: [%s]", err2str(res));
        return false;
    }
    res = avcodec_receive_packet(codec_ctx, out_pkt);
    if(res >= 0){
        return true;
    }
    else if(res == AVERROR(EAGAIN) || res == AVERROR_EOF){
        logging("cannot receive packet frame encoder this time, try again");
        return false;
    }
    else{
        logging("error while receive frame frome encoder: [%s]", err2str(res));
        return false;
    }
}

// 每种类型只记录第一个流
bool PackageLayer::FillDecoder(AVStream *stream)
{
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && v_codec_layer_.index == -1)
    {
        return v_codec_layer_.FillDecoder(stream);
    }
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && a_codec_layer_.index == -1)
    {
        return a_codec_layer_.FillDecoder(stream);
    }
    logging("this input stream [%d : %s] is not the type we need!",
            stream->index, av_get_media_type_string(stream->codecpar->codec_type));
    return true;
}

// copy不需要AVCodecContext
bool PackageLayer::FillEncoderCopyFrom(const CodecLayer &in_stream_ctx)
{
    bool ret = true;
    if (in_stream_ctx.stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        v_codec_layer_.stream = avformat_new_stream(avfmt, nullptr);
        ret = v_codec_layer_.FillEncoderCopyFrom(in_stream_ctx.stream->codecpar);
        if(!ret) logging("failed to copy video encoder!");
    }
    else if (in_stream_ctx.stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        a_codec_layer_.stream = avformat_new_stream(avfmt, nullptr);
        ret = a_codec_layer_.FillEncoderCopyFrom(in_stream_ctx.stream->codecpar);
        if(!ret) logging("failed to copy video encoder!");
    }
    else{
        logging("this input stream [%d : %s] is not the type we need!",
            in_stream_ctx.stream->index, av_get_media_type_string(in_stream_ctx.stream->codecpar->codec_type));
    }

    return ret;
}

bool PackageLayer::FillEncoderSetby(const CodecLayer &in_stream_ctx, const std::string& id)
{
    bool ret = true;
    if (in_stream_ctx.stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        v_codec_layer_.index = 0;
        v_codec_layer_.stream = avformat_new_stream(avfmt, nullptr);
        AVRational input_framerate = av_guess_frame_rate(nullptr, in_stream_ctx.stream, nullptr);
        ret = v_codec_layer_.FillVideoEncoder(in_stream_ctx.codec_ctx, id, input_framerate);
        if(!ret) logging("failed to fill video encoder!");
    }
    else if (in_stream_ctx.stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        a_codec_layer_.index = 1;
        a_codec_layer_.stream = avformat_new_stream(avfmt, nullptr);
        ret = a_codec_layer_.FillAudioEncoder(in_stream_ctx.codec_ctx, id);
        if(!ret) logging("failed to fill audio encoder!");
    }
    else{
        logging("this input stream [%d : %s] is not the type we need!",
            in_stream_ctx.index, av_get_media_type_string(in_stream_ctx.stream->codecpar->codec_type));
    }
    
    return ret;
}

bool PackageLayer::OpenFileAndInit()
{
    if(!avfmt->oformat){
        return false;
    }
    if(avfmt->oformat->flags & AVFMT_GLOBALHEADER){
        avfmt->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    if(!(avfmt->oformat->flags & AVFMT_NOFILE)){
        if(avio_open(&avfmt->pb, file_name.c_str(), AVIO_FLAG_WRITE) < 0){
            logging("failed to open output file!");
            return false; 
        }
    }
    if(avformat_write_header(avfmt, nullptr) < 0){
        logging("failed to write output header!");
        return false;
    }
    return true;
}

void PackageLayer::WirteTailAndClose()
{
    av_write_trailer(avfmt);
}

bool PackageLayer::ReadPacket(AVPacket *in_pkt)
{
    if(av_read_frame(avfmt, in_pkt) >= 0){
        return true;
    }
    else{
        return false;
    }
}

bool PackageLayer::WritePacket(AVPacket *out_pkt)
{   

    if(av_interleaved_write_frame(avfmt, out_pkt) >= 0){
        return true;
    }
    else{
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

    std::string &in_filename = input_package_layer_.file_name;
    std::string &out_filename = output_package_layer_.file_name;
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
    if(!output_package_layer_.OpenFileAndInit()){
        return false;
    }
    return true;
}

void Transcoder::Close()
{
    avformat_close_input(&input_package_layer_.avfmt);
    if (output_package_layer_.avfmt && !(output_package_layer_.avfmt->oformat->flags & AVFMT_NOFILE))
        avio_closep(&output_package_layer_.avfmt->pb);

    if(input_package_layer_.avfmt) avformat_free_context(input_package_layer_.avfmt);
    if(output_package_layer_.avfmt) avformat_free_context(output_package_layer_.avfmt);
    if(input_package_layer_.v_codec_layer_.codec_ctx) avcodec_free_context(&input_package_layer_.v_codec_layer_.codec_ctx);
    if(input_package_layer_.a_codec_layer_.codec_ctx) avcodec_free_context(&input_package_layer_.a_codec_layer_.codec_ctx); 
    if(output_package_layer_.v_codec_layer_.codec_ctx) avcodec_free_context(&output_package_layer_.v_codec_layer_.codec_ctx);
    if(output_package_layer_.a_codec_layer_.codec_ctx) avcodec_free_context(&output_package_layer_.a_codec_layer_.codec_ctx);
}

bool Transcoder::OpenInput()
{
    // open media
    input_package_layer_.avfmt = avformat_alloc_context();
    if (avformat_open_input(&input_package_layer_.avfmt, input_package_layer_.file_name.c_str(), 0, 0) < 0)
    {
        return false;
    }
    if (avformat_find_stream_info(input_package_layer_.avfmt, 0) < 0)
    {
        return false;
    }
    av_dump_format(input_package_layer_.avfmt, 0, input_package_layer_.file_name.c_str(), 0);

    // set decoder
    for (uint32_t i = 0; i < input_package_layer_.avfmt->nb_streams; i++)
    {
        AVStream *stream = input_package_layer_.avfmt->streams[i];
        input_package_layer_.FillDecoder(stream);
    }
    if (!input_package_layer_.v_codec_layer_.OpenCodec() || !input_package_layer_.a_codec_layer_.OpenCodec())
    {
        return false;
    }

    return true;
}

bool Transcoder::OpenOutput()
{
    avformat_alloc_output_context2(&output_package_layer_.avfmt, nullptr, nullptr, output_package_layer_.file_name.c_str());
    if (!output_package_layer_.avfmt)
    {
        logging("failed to alloce output format context!");
        return false;
    }
    if (input_package_layer_.v_codec_layer_.CodecExist())
    {
        if (v_copy_)
        {
            output_package_layer_.FillEncoderCopyFrom(input_package_layer_.v_codec_layer_);
        }
        else
        {   
            output_package_layer_.FillEncoderSetby(input_package_layer_.v_codec_layer_, v_encode_type_);
        }
    }
    if (input_package_layer_.a_codec_layer_.CodecExist())
    {
        if (a_copy_)
        {
            output_package_layer_.FillEncoderCopyFrom(input_package_layer_.a_codec_layer_);
        }
        else
        {
            output_package_layer_.FillEncoderSetby(input_package_layer_.a_codec_layer_, a_encode_type_);
        }
    }

    if(!v_copy_ && !output_package_layer_.v_codec_layer_.OpenCodec()){
        logging("failed to open video encoder!");
        return false;
    }
    if(!a_copy_ && !output_package_layer_.a_codec_layer_.OpenCodec()){
        logging("failed to open audio encoder!");
        return false;
    }

    av_dump_format(output_package_layer_.avfmt, 0, output_package_layer_.file_name.c_str(), 1);

    return true;
}

void Transcoder::Work()
{
    work_running_ = true;
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    while(work_running_){
        av_packet_unref(pkt);
        av_frame_unref(frame);
        if(input_package_layer_.ReadPacket(pkt)){
            if(AVMEDIA_TYPE_VIDEO == input_package_layer_.avfmt->streams[pkt->stream_index]->codecpar->codec_type){;
                if(!v_copy_){
                    if(!input_package_layer_.v_codec_layer_.Decode(pkt, frame)){
                        continue;
                    }
                    av_packet_unref(pkt);
                    if(output_package_layer_.v_codec_layer_.Encode(frame, pkt)){
                        continue;
                    }
                    av_frame_unref(frame);
                    pkt->stream_index = output_package_layer_.v_codec_layer_.index;
                    pkt->duration = output_package_layer_.v_codec_layer_.stream->time_base.den / 
                    output_package_layer_.v_codec_layer_.stream->time_base.num / 
                    input_package_layer_.v_codec_layer_.stream->avg_frame_rate.num * 
                    input_package_layer_.v_codec_layer_.stream->avg_frame_rate.den;
                }

                av_packet_rescale_ts(pkt, input_package_layer_.v_codec_layer_.stream->time_base,
                    output_package_layer_.v_codec_layer_.stream->time_base);
            }
            else if(AVMEDIA_TYPE_AUDIO == input_package_layer_.avfmt->streams[pkt->stream_index]->codecpar->codec_type){
                if(!a_copy_){
                    if(!input_package_layer_.a_codec_layer_.Decode(pkt, frame)){
                        continue;
                    }
                    av_packet_unref(pkt);
                    if(!output_package_layer_.a_codec_layer_.Encode(frame, pkt)){
                        continue;
                    }
                    av_frame_unref(frame);
                    pkt->stream_index = output_package_layer_.a_codec_layer_.index;
                }

                av_packet_rescale_ts(pkt, input_package_layer_.a_codec_layer_.stream->time_base,
                    output_package_layer_.a_codec_layer_.stream->time_base);
            }
            else{
                logging("not the type we need!");
                continue;
            }

            output_package_layer_.WritePacket(pkt);
        }
        else{
            break;
        }
    }   

    if(pkt) av_packet_free(&pkt);
    if(frame) av_frame_free(&frame);

    output_package_layer_.WirteTailAndClose();
    logging("write packet finished!");
}

void Transcoder::Start()
{
    work_running_ = true;
    work_thread_ = std::thread(&Transcoder::Work, this);
}

void Transcoder::Stop()
{
    work_running_ = false;
    if(work_thread_.joinable()){
        work_thread_.join();
    }
}

int main(int argc, char **argv)
{
    signal(SIGINT, int_handler);
    signal(SIGSEGV, segv_handler);
    Transcoder transcoder;
    if (!transcoder.ParseParam(argc, argv))
    {
        logging("param is not valid!");
        return -1;
    }
    if(!transcoder.Open()){
        return -1;
    }
    transcoder.Work();
    transcoder.Close();
    return 0;
}