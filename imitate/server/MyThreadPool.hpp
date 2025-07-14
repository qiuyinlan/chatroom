// imitate/server/MyThreadPool.hpp
// 最简版线程池实现，便于学习
#ifndef IMITATE_MYTHREADPOOL_HPP
#define IMITATE_MYTHREADPOOL_HPP
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>
#include <iostream>

// 线程池类：可以让你把任务交给线程池去做
class ThreadPool {
public:
    // 构造函数，初始化线程池，创建n个线程
    ThreadPool(size_t n) : stop(false) {
        for (size_t i = 0; i < n; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }
    // 添加任务到线程池
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }
    // 析构函数，销毁线程池
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
            worker.join();
    }
private:
    std::vector<std::thread> workers; // 工作线程
    std::queue<std::function<void()>> tasks; // 任务队列
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

#endif // IMITATE_MYTHREADPOOL_HPP 