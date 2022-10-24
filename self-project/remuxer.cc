#include <iostream>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <vector>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/frame.h"
#ifdef __cplusplus
}
#endif

int main(int argc, char **argv)
{
    int ret;
    const char *optstring = "i:o:h";
    std::string in_filename;
    std::string out_filename;
    while ((ret = getopt(argc, argv, optstring)) != -1)
    {
        switch (ret)
        {
        case 'i':
            in_filename = optarg;
            break;
        case 'o':
            out_filename = optarg;
            break;
        case 'h':
            printf("usage:\n remuxer -i [url] -o [url]\n");
            return 0;
        case '?':
            printf("error optopt: %c\n", optopt);
            printf("error opterr: %d\n", opterr);
            break;
        default:
            break;
        }
    }
    if (!in_filename.size() || !out_filename.size())
    {
        printf("please give a input url and a output url\n");
        return -1;
    }

    AVFormatContext *input_format_context = nullptr;
    AVFormatContext *output_format_context = nullptr;

    if (avformat_open_input(&input_format_context, in_filename.c_str(), nullptr, nullptr) < 0)
    {
        fprintf(stderr, "avformat_open_input failed!");
        return -1;
    }
    if ((ret = avformat_open_input(&input_format_context, in_filename.c_str(), NULL, NULL)) < 0)
    {
        fprintf(stderr, "Could not open input file '%s'", in_filename.c_str());
        return -1;
    }
    if ((ret = avformat_find_stream_info(input_format_context, NULL)) < 0)
    {
        fprintf(stderr, "Failed to retrieve input stream information");
        return -1;
    }

    avformat_alloc_output_context2(&output_format_context, NULL, NULL, out_filename.c_str());
    if (!output_format_context)
    {
        fprintf(stderr, "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        return -1;
    }
    int number_of_streams = input_format_context->nb_streams;
    std::vector<int> streams_list;
    int index = 0;
    for (int i = 0; i < input_format_context->nb_streams; i++)
    {
        AVStream *out_stream;
        AVStream *in_stream = input_format_context->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE)
        {
            streams_list.push_back(-1);
            continue;
        }
        streams_list.push_back(index++);
        out_stream = avformat_new_stream(output_format_context, NULL);
        if (!out_stream)
        {
            fprintf(stderr, "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            return -1;
        }
        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0)
        {
            fprintf(stderr, "Failed to copy codec parameters\n");
            return -1;
        }
    }
    av_dump_format(output_format_context, 0, out_filename.c_str(), 1);

    if (!(output_format_context->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&output_format_context->pb, out_filename.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stderr, "Could not open output file '%s'", out_filename);
            return -1;
        }
    }

    ret = avformat_write_header(output_format_context, nullptr);
    if (ret < 0)
    {
        fprintf(stderr, "Error occurred when opening output file\n");
        return -1;
    }

    AVPacket packet;
    while (1)
    {
        AVStream *in_stream, *out_stream;
        ret = av_read_frame(input_format_context, &packet);
        if (ret < 0)
            break;
        in_stream = input_format_context->streams[packet.stream_index];
        if (packet.stream_index >= number_of_streams || streams_list[packet.stream_index] < 0)
        {
            av_packet_unref(&packet);
            continue;
        }
        packet.stream_index = streams_list[packet.stream_index];
        out_stream = output_format_context->streams[packet.stream_index];
        /* copy packet */
        packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
        // https://ffmpeg.org/doxygen/trunk/structAVPacket.html#ab5793d8195cf4789dfb3913b7a693903
        packet.pos = -1;

        // https://ffmpeg.org/doxygen/trunk/group__lavf__encoding.html#ga37352ed2c63493c38219d935e71db6c1
        ret = av_interleaved_write_frame(output_format_context, &packet);
        if (ret < 0)
        {
            fprintf(stderr, "Error muxing packet\n");
            break;
        }
        av_packet_unref(&packet);
    }

    av_write_trailer(output_format_context);
    avformat_close_input(&input_format_context);
    if (output_format_context && !(output_format_context->oformat->flags & AVFMT_NOFILE))
        avio_closep(&output_format_context->pb);
    avformat_free_context(output_format_context);
    return 0;
}