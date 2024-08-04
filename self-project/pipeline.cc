#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <memory>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#ifdef __cplusplus
}
#endif

using ByteBuffer = std::vector<uint8_t>;

class Plugin;
using PluginPtr = std::shared_ptr<Plugin>;
class ThreadPoll
{
public:
    static ThreadPoll &Instance()
    {
        static ThreadPoll tp;
        return tp;
    }

    void Enqueue(Plugin *plg)
    {
        std::lock_guard<std::mutex> lkg(mu_);
        plugins_.push(plg);
        cv_.notify_one();
    }

    ~ThreadPoll()
    {
        
    }

    void Start(int thread_num)
    {
        for (int i = 0; i < thread_num; ++i)
        {
            threads_.emplace_back([this](){
                for(;;){
                    Plugin *plg = nullptr;
                    {
                        std::unique_lock<std::mutex> ulk(mu_);
                        cv_.wait(ulk, [this](){return thd_exit_ || !plugins_.empty();});
                        if (thd_exit_)
                            return;

                        if(!plugins_.empty()){
                            plg = plugins_.front();
                            plugins_.pop();
                        }
                    }
                    if(plg){
                        plg->Run();
                    }
                    {
                        std::lock_guard<std::mutex> lkg(mu_);
                        plugins_.push(plg);
                    }
                    cv_.notify_one();
                } });
        }
    }

    void Stop()
    {
        {
            std::lock_guard<std::mutex> lkg(mu_);
            thd_exit_ = true;
        }
        cv_.notify_all();
        for (auto &thd : threads_)
        {
            if (thd.joinable())
                thd.join();
        }
    }

private:
    ThreadPoll() = default;

private:
    std::vector<std::thread> threads_;
    std::queue<Plugin *> plugins_;
    bool thd_exit_{false};
    std::mutex mu_;
    std::condition_variable cv_;
};

class Plugin
{
public:
    virtual ~Plugin() = default;
    virtual bool Init(Plugin *next = nullptr)
    {
        next_ = next;
        ThreadPoll::Instance().Enqueue(this);
        return true;
    }
    virtual void Deinit()
    {
        next_ = nullptr;
        {
            std::lock_guard<std::mutex> lkg(mu_);
            buf_list_.clear();
        }
    }
    virtual void Run() = 0;

    void Enqueue(ByteBuffer &&buf)
    {
        {
            std::lock_guard<std::mutex> lkg(mu_);
            buf_list_.emplace_back(std::move(buf));
        }
        cv_.notify_one();
    }

protected:
    bool Dequeue(ByteBuffer &buf)
    {
        std::unique_lock<std::mutex> ulk(mu_);
        cv_.wait(ulk, [this](){return !buf_list_.empty();});
        if (buf_list_.empty())
            return false;
        buf.swap(buf_list_.front());
        buf_list_.pop_front();
        return true;
    }

    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<ByteBuffer> buf_list_;
    Plugin *next_{nullptr};
};

class ReadPlugin : public Plugin
{
private:
    enum
    {
        kInit = 0,
        kRead,
        kEOF
    }state_;
public:
    ~ReadPlugin() override
    {

    }
    bool Init(const std::string &filename, Plugin *next)
    {
        filename_ = filename;
        state_ = kInit;
        return Plugin::Init(next);
    }
    void Deinit() override
    {

    }
    void Run() override
    {
        switch (state_)
        {
        case kInit:{
            if (0 != avformat_open_input(&fmt_ctx_, filename_.c_str(), NULL, NULL))
            {
                fprintf(stderr, "Could not open input file '%s'", filename_.c_str());
                return;
            }
            if (0 != avformat_find_stream_info(fmt_ctx_, NULL))
            {
                fprintf(stderr, "Failed to retrieve input stream information");
                return;
            }
            av_dump_format(fmt_ctx_, 0, filename_.c_str(), 0);
            state_ = kRead;
        }
            break;
        case kRead:{
            int ret = 0;
            if((ret = av_read_frame(fmt_ctx_, &pkt_)) != 0){
                if(ret == AVERROR_EOF){
                    state_ = kEOF;
                }
                return;
            }
            buf_.resize(pkt_.size);
            memcpy(buf_.data(), pkt_.data, pkt_.size);
            if(next_){
                next_->Enqueue(std::move(buf_));
            }
        }
            break;
        case kEOF:
            fprintf(stderr, "End of file");
            break;
        default:
            break;
        }
    }
private:
    std::string filename_;
    AVFormatContext *fmt_ctx_{nullptr};
    AVPacket pkt_;
    ByteBuffer buf_;
};

class WritePlugin : public Plugin
{
public:
    ~WritePlugin() override
    {
    }
    void Deinit() override
    {

    }
    void Run() override
    {
    }
};

class DemuxPlugin : public Plugin
{
public:
    ~DemuxPlugin() override
    {
    }
    void Deinit() override
    {

    }
    void Run() override
    {
        ByteBuffer buf;
        if(Dequeue(buf)){
            fprintf(stdout, "buf len = %ull", buf.size());
        }
    }
};

class MuxPlugin : public Plugin
{
    ~MuxPlugin() override
    {
    }
    void Deinit() override
    {
        
    }
    void Run() override
    {
    }
};


int 
main()
{
    ThreadPoll::Instance().Start(4);
    ReadPlugin read;
    DemuxPlugin demux;

    read.Init("test.mp4", &demux);
    demux.Init();

    usleep(10 * 1000);
    ThreadPoll::Instance().Stop();
}