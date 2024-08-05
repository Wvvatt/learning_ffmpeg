#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <iostream>
#include <unistd.h>
#include <chrono>

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

using ExecutionState = bool;
constexpr bool kBusying = true;
constexpr bool kIdle = false;

class Plugin
{
public:
    virtual ~Plugin()
    {
        Deinit();
    }

    virtual bool Init(Plugin *next = nullptr)
    {
        next_ = next;
        return true;
    }

    virtual void Deinit()
    {
        next_ = nullptr;
        buf_list_.clear();
    }

    virtual ExecutionState Run() = 0;

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
        cv_.wait_for(ulk, std::chrono::milliseconds(1), [this](){return !buf_list_.empty();});
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

class DemuxPlugin : public Plugin
{
private:
    enum
    {
        kInit = 0,
        kRead,
        kEOF
    }state_;

public:
    ~DemuxPlugin() override
    {

    }
    bool Init(Plugin *next, const std::string &filename)
    {
        filename_ = filename;
        state_ = kInit;
        return Plugin::Init(next);
    }
    void Deinit() override
    {

    }
    ExecutionState Run() override
    {
        switch (state_)
        {
        case kInit:{
            if (0 != avformat_open_input(&fmt_ctx_, filename_.c_str(), NULL, NULL))
            {
                std::cerr << "Could not open input file " << filename_ << std::endl;
                return kIdle;
            }
            if (0 != avformat_find_stream_info(fmt_ctx_, NULL))
            {
                std::cerr << "Failed to retrieve input stream information" << std::endl;
                return kIdle;
            }
            av_dump_format(fmt_ctx_, 0, filename_.c_str(), 0);
            state_ = kRead;
        }
            return kBusying;
        case kRead:{
            int ret = 0;
            if((ret = av_read_frame(fmt_ctx_, &pkt_)) != 0){
                if(ret == AVERROR_EOF){
                    std::cout << "read EOF" << std::endl;
                    state_ = kEOF;
                }
                return kIdle;
            }
            buf_.resize(pkt_.size);
            memcpy(buf_.data(), pkt_.data, pkt_.size);
            if(next_){
                next_->Enqueue(std::move(buf_));
            }
        }
            return kBusying;
        case kEOF:
        default:
            return kIdle;
        }
    }
private:
    std::string filename_;
    AVFormatContext *fmt_ctx_{nullptr};
    AVPacket pkt_;
    ByteBuffer buf_;
};

class MuxPlugin : public Plugin
{
public:
    ~MuxPlugin() override
    {

    }

    bool Init(Plugin *next, const std::string &mux_mode)
    {
        mux_mode_ = mux_mode;
        Plugin::Init(next);
        return true;
    }

    void Deinit() override
    {
        
    }

    ExecutionState Run() override
    {
        ByteBuffer buf;
        if(Dequeue(buf)){
            // std::cout << "read data size = " << buf.size() << std::endl;
            return kBusying;
        }
        return kIdle;
    }

private:
    std::string mux_mode_;
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
    ExecutionState Run() override
    {
        return kIdle;
    }
};