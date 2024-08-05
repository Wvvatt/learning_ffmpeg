#pragma once
#include <thread>
#include <mutex>
#include <functional>
#include <vector>
#include <queue>
#include <condition_variable>

using TaskFunc = std::function<bool()>;

class ThreadPoll
{
public:
    static ThreadPoll &Instance()
    {
        static ThreadPoll tp;
        return tp;
    }

    ~ThreadPoll()
    {
    }

    void AddTask(TaskFunc func)
    {
        std::lock_guard<std::mutex> lkg(mu_);
        tasks_.push(func);
        cv_.notify_one();
    }

    void Start(int thread_num)
    {
        for (int i = 0; i < thread_num; ++i)
        {
            threads_.emplace_back([this]()
            {
                size_t idle = 0;
                for(;;){
                    TaskFunc task = nullptr;
                    {
                        std::unique_lock<std::mutex> ulk(mu_);
                        cv_.wait(ulk, [this](){return this->thd_exit_ || !this->tasks_.empty();});
                        if (this->thd_exit_)
                            return;

                        if(!this->tasks_.empty()){
                            task = this->tasks_.front();
                            this->tasks_.pop();
                        }
                    }
                    if(!task){
                        continue;
                    }
                    if(task()){
                        idle = 0;
                    }
                    else{
                        ++idle;
                    }
                    {
                        std::lock_guard<std::mutex> lkg(mu_);
                        this->tasks_.push(task);
                    }
                    cv_.notify_one();
                    if(idle > 3){
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                } 
            }
            );
        }
    }

    void Stop()
    {
        {
            std::lock_guard<std::mutex> lkg(mu_);
            thd_exit_ = true;
        }
        cv_.notify_all();
        for (auto &&thd : threads_)
        {
            if (thd.joinable())
                thd.join();
        }
        threads_.clear();
    }

private:
    ThreadPoll() = default;

private:
    std::vector<std::thread> threads_;
    std::queue<TaskFunc> tasks_;
    bool thd_exit_{false};
    std::mutex mu_;
    std::condition_variable cv_;
};