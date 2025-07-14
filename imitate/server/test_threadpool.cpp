// imitate/server/test_threadpool.cpp
// 演示如何使用最简版线程池
#include "MyThreadPool.hpp"
#include <iostream>
#include <vector>

int main() {
    ThreadPool pool(3); // 创建3个线程的线程池
    std::vector<std::future<int>> results;
    // 添加10个任务到线程池
    for (int i = 0; i < 10; ++i) {
        results.emplace_back(
            pool.enqueue([i] {
                std::cout << "任务 " << i << " 正在运行，线程ID: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return i * i;
            })
        );
    }
    // 获取任务结果
    for (auto &&result : results)
        std::cout << "结果: " << result.get() << std::endl;
    std::cout << "所有任务完成！" << std::endl;
    return 0;
} 