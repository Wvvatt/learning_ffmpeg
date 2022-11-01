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

bool CodecContext::FillDecoder(AVStream *in_stream)
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

bool CodecContext::FillEncoderCopyFrom(AVCodecParameters *in_codecpar)
{
    if(avcodec_parameters_copy(stream->codecpar, in_codecpar) < 0)
    {
        return false;
    }
    return true;
}

bool CodecContext::FillVideoEncoder(AVCodecContext *dec_ctx, const std::string& id, AVRational frame_rate)
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

    codec_ctx->time_base = av_inv_q(frame_rate);
    stream->time_base = codec_ctx->time_base;

    avcodec_parameters_from_context(stream->codecpar, codec_ctx);

    return true;
}


bool CodecContext::FillAudioEncoder(AVCodecContext *dec_ctx, const std::string& id)
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

    codec_ctx->sample_rate = dec_ctx->sample_rate;
    codec_ctx->sample_fmt = dec_ctx->sample_fmt;
    codec_ctx->bit_rate = dec_ctx->bit_rate;

    codec_ctx->time_base = (AVRational){1, dec_ctx->sample_rate};

    codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    stream->time_base = codec_ctx->time_base;

    avcodec_parameters_from_context(stream->codecpar, codec_ctx);

    return true;
}

bool CodecContext::OpenCodec()
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

bool CodecContext::Exist() const
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

bool CodecContext::Decode(AVPacket *in_pkt, AVFrame *out_frame)
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

bool CodecContext::Encode(AVFrame *in_frame, AVPacket *out_pkt)
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
bool PackageContext::FillDecoder(AVStream *stream)
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
bool PackageContext::FillEncoderCopyFrom(const CodecContext &in_stream_ctx)
{
    bool ret = true;
    if (in_stream_ctx.stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        v_stream_ctx.stream = avformat_new_stream(avfmt, nullptr);
        ret = v_stream_ctx.FillEncoderCopyFrom(in_stream_ctx.stream->codecpar);
        if(!ret) logging("failed to copy video encoder!");
    }
    else if (in_stream_ctx.stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        a_stream_ctx.stream = avformat_new_stream(avfmt, nullptr);
        ret = a_stream_ctx.FillEncoderCopyFrom(in_stream_ctx.stream->codecpar);
        if(!ret) logging("failed to copy video encoder!");
    }
    else{
        logging("this input stream [%d : %s] is not the type we need!",
            in_stream_ctx.stream->index, av_get_media_type_string(in_stream_ctx.stream->codecpar->codec_type));
    }

    return ret;
}

bool PackageContext::FillEncoderSetby(const CodecContext &in_stream_ctx, const std::string& id)
{
    bool ret = true;
    if (in_stream_ctx.stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        v_stream_ctx.index = 0;
        v_stream_ctx.stream = avformat_new_stream(avfmt, nullptr);
        AVRational input_framerate = av_guess_frame_rate(nullptr, in_stream_ctx.stream, nullptr);
        ret = v_stream_ctx.FillVideoEncoder(in_stream_ctx.codec_ctx, id, input_framerate);
        if(!ret) logging("failed to fill video encoder!");
    }
    else if (in_stream_ctx.stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        a_stream_ctx.index = 1;
        a_stream_ctx.stream = avformat_new_stream(avfmt, nullptr);
        ret = a_stream_ctx.FillAudioEncoder(in_stream_ctx.codec_ctx, id);
        if(!ret) logging("failed to fill audio encoder!");
    }
    else{
        logging("this input stream [%d : %s] is not the type we need!",
            in_stream_ctx.index, av_get_media_type_string(in_stream_ctx.stream->codecpar->codec_type));
    }
    
    return ret;
}

bool PackageContext::OpenFileAndInit()
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

void PackageContext::WirteTailAndClose()
{
    av_write_trailer(avfmt);
}

bool PackageContext::ReadPacket(AVPacket *in_pkt)
{
    if(av_read_frame(avfmt, in_pkt) >= 0){
        return true;
    }
    else{
        return false;
    }
}

bool PackageContext::WritePacket(AVPacket *out_pkt)
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
    if(!output_fmtctx_.OpenFileAndInit()){
        return false;
    }
    work_thread_ = std::thread(&Transcoder::Work, this);
    return true;
}

void Transcoder::Close()
{
    avformat_close_input(&input_fmtctx_.avfmt);
    if (output_fmtctx_.avfmt && !(output_fmtctx_.avfmt->oformat->flags & AVFMT_NOFILE))
        avio_closep(&output_fmtctx_.avfmt->pb);

    if(input_fmtctx_.avfmt) avformat_free_context(input_fmtctx_.avfmt);
    if(output_fmtctx_.avfmt) avformat_free_context(output_fmtctx_.avfmt);
    if(input_fmtctx_.v_stream_ctx.codec_ctx) avcodec_free_context(&input_fmtctx_.v_stream_ctx.codec_ctx);
    if(input_fmtctx_.a_stream_ctx.codec_ctx) avcodec_free_context(&input_fmtctx_.a_stream_ctx.codec_ctx); 
    if(output_fmtctx_.v_stream_ctx.codec_ctx) avcodec_free_context(&output_fmtctx_.v_stream_ctx.codec_ctx);
    if(output_fmtctx_.a_stream_ctx.codec_ctx) avcodec_free_context(&output_fmtctx_.a_stream_ctx.codec_ctx);
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
            output_fmtctx_.FillEncoderCopyFrom(input_fmtctx_.v_stream_ctx);
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
            output_fmtctx_.FillEncoderCopyFrom(input_fmtctx_.a_stream_ctx);
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

    av_dump_format(output_fmtctx_.avfmt, 0, output_fmtctx_.file_name.c_str(), 1);

    return true;
}

void Transcoder::Work()
{
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    while(work_running_){
        av_packet_unref(pkt);
        av_frame_unref(frame);
        if(input_fmtctx_.ReadPacket(pkt)){
            if(input_fmtctx_.avfmt->streams[pkt->stream_index]->codecpar->codec_type 
            == AVMEDIA_TYPE_VIDEO){
                auto&& stream_ctx = output_fmtctx_.v_stream_ctx;
                if(!v_copy_){
                    if(stream_ctx.Decode(pkt, frame)){
                        continue;
                    }
                    av_packet_unref(pkt);
                    stream_ctx.Encode(frame, pkt);
                    av_frame_unref(frame);
                    pkt->stream_index = stream_ctx.index;
                    pkt->duration = stream_ctx.stream->time_base.den / 
                    stream_ctx.stream->time_base.num / 
                    input_fmtctx_.v_stream_ctx.stream->avg_frame_rate.num * 
                    input_fmtctx_.v_stream_ctx.stream->avg_frame_rate.den;
                }

                av_packet_rescale_ts(pkt, input_fmtctx_.v_stream_ctx.stream->time_base,
                    output_fmtctx_.v_stream_ctx.stream->time_base);
            }
            else if(input_fmtctx_.avfmt->streams[pkt->stream_index]->codecpar->codec_type 
            == AVMEDIA_TYPE_AUDIO){
                auto &&stream_ctx = output_fmtctx_.a_stream_ctx;
                if(!a_copy_){
                    stream_ctx.Decode(pkt, frame);
                    av_packet_unref(pkt);
                    stream_ctx.Encode(frame, pkt);
                    av_frame_unref(frame);
                    pkt->stream_index = stream_ctx.index;
                }

                av_packet_rescale_ts(pkt, input_fmtctx_.a_stream_ctx.stream->time_base,
                    output_fmtctx_.a_stream_ctx.stream->time_base);
            }
            else{
                logging("not the type we need!");
                continue;
            }

            output_fmtctx_.WritePacket(pkt);
        }
        else{
            break;
        }
    }   

    if(pkt) av_packet_free(&pkt);
    if(frame) av_frame_free(&frame);
}

void Transcoder::Start()
{
    work_running_ = true;
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
    transcoder.Start();
    sem_wait(&sem);
    transcoder.Stop();
    transcoder.Close();
    return 0;
}