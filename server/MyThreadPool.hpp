// MyThreadPool.hpp
// 整合自ThreadPool.hpp、ThreadPool.tpp、ThreadPool.cc


#ifndef CHATROOM_MYTHREADPOOL_HPP
#define CHATROOM_MYTHREADPOOL_HPP

#include <queue>
#include <future>
#include <optional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <chrono>
#include <iostream>

// 线程池类，支持动态扩容和收缩，支持添加任意类型的任务
class ThreadPool {
public:
    // 构造函数，指定最小和最大线程数
    ThreadPool(size_t minNum, size_t maxNum) noexcept;
    // 析构函数，安全关闭线程池
    ~ThreadPool() noexcept;

    // 模板函数：添加任意类型的任务到线程池
    // Func: 可调用对象类型（函数、lambda等）
    // Args: 任务参数类型
    template<typename Func, typename...Args>
    auto addTask(Func &&func, Args &&...args) -> std::optional<std::future<decltype(func(args...))>>;

private:
    void worker();           // 工作线程主循环
    void manager();          // 管理线程主循环
    void addWorkers();       // 扩容线程池（未使用）
    void removeIdleWorkers();// 收缩线程池（未使用）

private:
    std::mutex pool_lock;
    std::condition_variable not_empty;
    std::vector<std::thread> threads;      // 工作线程
    std::thread manager_thread;            // 管理线程
    std::queue<std::function<void()>> tasks; // 任务队列
    const size_t min_num;                  // 最小线程数
    const size_t max_num;                  // 最大线程数
    std::atomic<size_t> busy_num;          // 正在工作的线程数
    std::atomic<size_t> alive_num;         // 存活线程数
    std::atomic<bool> shutdown;            // 线程池是否关闭
    std::atomic<bool> manager_stopped;     // 管理线程是否停止
};

// ================== 模板成员函数实现 ==================
// addTask: 添加任意类型的任务到线程池
// 参考《C++ Primer》第16章模板与泛型编程
//
template<typename Func, typename...Args>
auto ThreadPool::addTask(Func &&func, Args &&...args) -> std::optional<std::future<decltype(func(args...))>> {
    if (shutdown) {
        return std::nullopt;
    }
    using return_type = decltype(func(args...));
    auto task = std::make_shared<std::packaged_task<return_type()>>(
            [func, args...]() mutable {
                return func(args...);
            }
    );
    std::future<return_type> res = task->get_future();
    {
        std::lock_guard<std::mutex> lock(pool_lock);
        tasks.emplace([task]() { (*task)(); });
    }
    not_empty.notify_one();
    return res;
}

// ================== 普通成员函数实现 ==================
// 构造函数
inline ThreadPool::ThreadPool(size_t minNum, size_t maxNum) noexcept
        : min_num(minNum), max_num(maxNum), busy_num(0), alive_num(minNum),
          shutdown(false), manager_stopped(false) {
    for (size_t i = 0; i < minNum; i++) {
        threads.emplace_back(&ThreadPool::worker, this);
    }
    manager_thread = std::thread(&ThreadPool::manager, this);
}

// 析构函数
inline ThreadPool::~ThreadPool() noexcept {
    manager_stopped = true;
    if (manager_thread.joinable()) {
        manager_thread.join();
    }
    shutdown = true;
    not_empty.notify_all();
    for (auto &t: threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

// 工作线程主循环
inline void ThreadPool::worker() {
    std::function<void()> task;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(pool_lock);
            not_empty.wait(lock, [this] { return shutdown || !tasks.empty(); });
            if (shutdown && tasks.empty()) {
                alive_num--;
                return;
            }
            task = std::move(tasks.front());
            tasks.pop();
            busy_num++;
        }
        task();
        busy_num--;
    }
}

// 管理线程主循环
inline void ThreadPool::manager() {
    while (!manager_stopped) {
        std::unique_lock<std::mutex> lock(pool_lock);
        not_empty.wait_for(lock, std::chrono::seconds(3), [this] {
            return shutdown || manager_stopped || (tasks.size() > alive_num && alive_num < max_num) ||
                   (busy_num * 2 < alive_num && alive_num > min_num);
        });
        if (!shutdown && !manager_stopped) {
            while (tasks.size() > alive_num && alive_num < max_num) {
                std::cout << "扩容一个线程.." << std::endl;
                threads.emplace_back(&ThreadPool::worker, this);
                alive_num++;
            }
            while (busy_num * 2 < alive_num && alive_num > min_num) {
                std::cout << "销毁一个线程..." << std::endl;
                for (auto it = threads.begin(); it != threads.end(); it++) {
                    if (!it->joinable()) {
                        it = threads.erase(it);
                        alive_num--;
                        break;
                    }
                }
            }
        }
    }
}

#endif //CHATROOM_MYTHREADPOOL_HPP 