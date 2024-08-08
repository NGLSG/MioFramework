//
// Created by 92703 on 2024/7/29.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>


class ThreadPool {
public:
    using Task = std::function<void()>;

    ThreadPool(size_t numThreads);

    ~ThreadPool();

    template<class F, class... Args>
    auto enqueue(F&&f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

    void waitAll() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        condition.wait(lock, [this] { return tasks.empty(); });
    }

private:
    std::vector<std::thread> workers;
    std::queue<Task> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

template<class F, class... Args>
auto ThreadPool::enqueue(F&&f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    std::future<return_type> res = task->get_future(); {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (tasks.empty()) {
            condition.notify_one();
        }
        tasks.push([task]() { (*task)(); });
    }
    return res;
}

#endif //THREADPOOL_H
