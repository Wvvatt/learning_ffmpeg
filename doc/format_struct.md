[toc]

## AVFormatContext

#### 使用c++类表示
```cpp
// 抽象接口类
class AVInputFormatBase
{
public:
    virtual int read_probe() = 0;
    virtual int read_header() = 0;
    ...
private:
    const char *name;
    ...
}
// 子类实现
class MpegTsInputFormat : public AVInputFormatBase
{
public:
    int read_probe() override;
    int read_header() override;
}

// 抽象接口类
class AVOutputFormatBase
{
public:
    virtual int write_header() = 0;
    virtual int write_packet() = 0;
    ...
private:
    const char *name;
    ...
}

class AVFormatContext
{
public:
    AVInputFormatBase *input_fmt;       // 方法使用抽象接口
    AVOutputFormatBase *output_fmt;     // 
    ...
private:
    AVIOContext *pb;                    // 成员自己保存
    unsigned int nb_streams;
    AVStream **streams;
    char *url;
    ...
}
```


#### 结构

## AVInputFormat

#### 读取过程
读文件->mpegts解封装堆栈如下
```
file_read(URLContext * h, unsigned char * buf, int size) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\file.c:113)
retry_transfer_wrapper(URLContext * h, uint8_t * buf, int size, int size_min, int (*)(URLContext *, uint8_t *, int) transfer_func) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\avio.c:370)
ffurl_read(URLContext * h, unsigned char * buf, int size) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\avio.c:405)
read_packet_wrapper(AVIOContext * s, uint8_t * buf, int size) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\aviobuf.c:533)
fill_buffer(AVIOContext * s) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\aviobuf.c:577)
avio_read(AVIOContext * s, unsigned char * buf, int size) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\aviobuf.c:672)
ffio_read_indirect(AVIOContext * s, unsigned char * buf, int size, const unsigned char ** data) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\aviobuf.c:709)
read_packet(AVFormatContext * s, uint8_t * buf, int raw_packet_size, const uint8_t ** data) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\mpegts.c:2942)
handle_packets(MpegTSContext * ts, int64_t nb_packets) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\mpegts.c:3009)
mpegts_read_packet(AVFormatContext * s, AVPacket * pkt) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\mpegts.c:3256)
ff_read_packet(AVFormatContext * s, AVPacket * pkt) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\demux.c:571)
read_frame_internal(AVFormatContext * s, AVPacket * pkt) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\demux.c:1245)
av_read_frame(AVFormatContext * s, AVPacket * pkt) (\home\watt\learning_ffmpeg\ffmpeg-5.1\libavformat\demux.c:1450)
```
- `ff_read_packet`调用了`s->iformat->read_packet`，这是个接口，挂上了`ff_mpegts_demuxer.mpegts_read_packet`
- `ff_mpegts_demuxer.mpegts_read_packet`读取buffer内容时调用`avio_read`，一直调用到`(AVIOContext)s.read_packet`，这是个函数指针，挂上URLContext的`ffurl_read`，最后调用到`(URLContext)h.prot.url_read`，这是个接口，挂上`ff_file_protocol.file_read`

#### 结构
```c
typedef struct AVInputFormat {
    const char *name;
    const char *long_name;
    int flags;
    const char *extensions;
    const struct AVCodecTag * const *codec_tag;
    const AVClass *priv_class; ///< AVClass for the private context
    const char *mime_type;
    int raw_codec_id;
    int priv_data_size;
    int flags_internal;
    int (*read_probe)(const AVProbeData *);
    int (*read_header)(struct AVFormatContext *);
    int (*read_packet)(struct AVFormatContext *, AVPacket *pkt);
    int (*read_close)(struct AVFormatContext *);
    int (*read_seek)(struct AVFormatContext *,
                     int stream_index, int64_t timestamp, int flags);
    int64_t (*read_timestamp)(struct AVFormatContext *s, int stream_index,
                              int64_t *pos, int64_t pos_limit);
    int (*read_play)(struct AVFormatContext *);
    int (*read_pause)(struct AVFormatContext *);
    int (*read_seek2)(struct AVFormatContext *s, int stream_index, int64_t min_ts, int64_t ts, int64_t max_ts, int flags);
    int (*get_device_list)(struct AVFormatContext *s, struct AVDeviceInfoList *device_list);
} AVInputFormat;
```

## AVOutputFormat


