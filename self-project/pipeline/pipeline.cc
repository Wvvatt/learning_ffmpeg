#include <unistd.h>
#include "thread_poll.h"
#include "plugin.h"

int 
main()
{
    ThreadPoll::Instance().Start(4);
    DemuxPlugin demux;
    MuxPlugin mux;

    demux.Init(&mux, "./data/avc_720p_aac_4m_bmw.mp4");
    mux.Init(nullptr, "");

    ThreadPoll::Instance().AddTask([&demux]() -> bool{
        return demux.Run();
    });

    ThreadPoll::Instance().AddTask([&mux]() -> bool{
        return mux.Run();
    });

    std::this_thread::sleep_for(std::chrono::seconds(5));

    ThreadPoll::Instance().Stop();
}