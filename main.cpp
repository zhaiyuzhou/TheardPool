#include <iostream>
#include <thread>
#include <mutex>
#include <string>
#include <condition_variable>
#include <queue>
#include <vector>
#include <functional>


class ThreadPool {
private:

    // 线程表
    std::vector<std::thread> threads;
    // 任务队列
    std::queue<std::function<void()>> tasks;
    // 队列互斥锁
    std::mutex queue_mutex;
    // 消费者模版
    std::condition_variable condition;
    // 是否停止线程池
    bool stop;
public:
    // 创建numThreads个线程
    ThreadPool(int numThreads) : stop(false) {
        for (int i = 0; i < numThreads; i++){
            threads.emplace_back([this]{
                while (1) {
                    // 加锁
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    // 在任务队列不为空或者停止线程池时，等待
                    condition.wait(lock,[this]{
                        return !tasks.empty() || stop;
                    });
                    // 在任务队列空并且停止线程池时结束
                    if(stop && tasks.empty()) {
                        return;
                    }

                    // 取出任务列表的任务执行
                    std::function<void()> task(std::move(tasks.front()));
                    tasks.pop();
                    lock.unlock();
                    task();
                }
            });
        }
    }

    ~ThreadPool(){
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        // 结束时通知任务执行
        condition.notify_all();
        for(auto& t : threads){
            // 等待全部任务执行接触
            t.join();
        }
    }

    template<class F, class... Args>
    void enqueue(F &&f,Args&&...args){
        std::function<void()>task = std::bind(std::forward<F>(f),std::forward<Args>(args)...);
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            // 放入任务队列
            tasks.emplace(task);
        }
        // 通知任务执行
        condition.notify_one(); // notify one waiting thread
    }

};

int main() {
    ThreadPool pool(4);
    std::mutex cout_mutex;
    for (int i = 0; i < 8; i++) {
        pool.enqueue([i,&cout_mutex]() {
            {
                std::lock_guard<std::mutex> lock(cout_mutex); // 在写入 std::cout 之前锁定互斥锁
                std::cout << "Task " << i << " is running." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            {
                std::lock_guard<std::mutex> lock(cout_mutex); // 在写入 std::cout 之前锁定互斥锁
                std::cout << "Task " << i << " is done." << std::endl;
            }
        });
    }
    return 0;
}
