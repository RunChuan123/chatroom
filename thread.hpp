#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>
#include <chrono>
#include <functional>
#include <atomic>
#include <future>
#include <memory>
#include "debug_logger.hpp"   // 日志类


template <typename T>
class SafeQueue {
public:
    std::queue<T> s_queue;
    std::mutex s_mu;
    std::condition_variable s_cv;

    SafeQueue() {
        LOG_DEBUG("SafeQueue created");
    }

    SafeQueue(SafeQueue&& other) {
        LOG_DEBUG("SafeQueue moved");
    }

    ~SafeQueue() {
        LOG_DEBUG("SafeQueue destroyed");
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(s_mu);
        return s_queue.empty();
    }

    int size() {
        std::lock_guard<std::mutex> lock(s_mu);
        return (int)s_queue.size();
    }

    template <typename U>
    void enqueue(U&& t) {
        {
            std::lock_guard<std::mutex> lock(s_mu);
            s_queue.emplace(std::forward<U>(t));
            LOG_DEBUG("Task enqueued, queue size=", s_queue.size());
        }
        s_cv.notify_one();
    }

    bool dequeue(T& t, bool wait = false, bool is_shutdown = false) {
        auto pop_one = [&](T& t) {
            t = std::move(s_queue.front());
            s_queue.pop();
        };

        if (wait) {
            std::unique_lock<std::mutex> lock(s_mu);
            s_cv.wait(lock, [this, is_shutdown] { return is_shutdown || !s_queue.empty(); });
            if (!s_queue.empty()) {
                pop_one(t);
                LOG_DEBUG("Task dequeued (wait=true), queue size=", s_queue.size());
                return true;
            }
            LOG_WARN("Dequeue failed: shutdown or empty queue");
            return false;
        } else {
            std::lock_guard<std::mutex> lock(s_mu);
            if (s_queue.empty()) {
                LOG_WARN("Dequeue (wait=false) failed: queue empty");
                return false;
            }
            pop_one(t);
            LOG_DEBUG("Task dequeued (wait=false), queue size=", s_queue.size());
            return true;
        }
    }
};

class ThreadPool {
public:
    // 内部线程工作类
    class ThreadWorker {
    private:
        int w_id;
        ThreadPool* w_pool;
    public:
        ThreadWorker(ThreadPool* pool, const int id) : w_id(id), w_pool(pool) {
            LOG_DEBUG("ThreadWorker ", id, " created");
        }

        void operator()() {
            LOG_DEBUG("Worker ", w_id, " started");
            std::function<void()> func;
            bool dequeued;

            while (!w_pool->is_shutdown) {
                dequeued = w_pool->task_queue.dequeue(func, true, w_pool->is_shutdown);

                if (!dequeued) {
                    LOG_DEBUG("Worker ", w_id, " exiting loop");
                    break;
                }

                LOG_DEBUG("Worker ", w_id, " executing task");
                try {
                    func();
                    LOG_DEBUG("Worker ", w_id, " finished task");
                } catch (const std::exception& e) {
                    LOG_ERROR("Worker ", w_id, " task threw exception: ", e.what());
                } catch (...) {
                    LOG_ERROR("Worker ", w_id, " task threw unknown exception");
                }
            }

            LOG_DEBUG("Worker ", w_id, " stopped");
        }
    };

    std::atomic<bool> is_shutdown{false};
    SafeQueue<std::function<void()>> task_queue;
    std::vector<std::thread> work_thread;

    ThreadPool(const int num_workers)
        : is_shutdown(false), work_thread(std::vector<std::thread>(num_workers)) {
        LOG_INFO("ThreadPool created with ", num_workers, " workers");
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    void init() {
        for (std::size_t i = 0; i < work_thread.size(); i++) {
            work_thread[i] = std::thread(ThreadWorker(this, (int)i));
            LOG_INFO("Thread ", i, " started");
        }
    }

    void shutdown() {
        LOG_WARN("ThreadPool shutting down");
        is_shutdown = true;
        {
            std::unique_lock<std::mutex> lock(task_queue.s_mu);
            task_queue.s_cv.notify_all();
        }
        for (std::size_t i = 0; i < work_thread.size(); i++) {
            if (work_thread[i].joinable()) {
                work_thread[i].join();
                LOG_INFO("Thread ", i, " joined");
            }
        }
        LOG_INFO("ThreadPool shutdown complete");
    }

    // 提交任务
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using RetType = decltype(f(args...));

        std::function<RetType()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        auto task_ptr = std::make_shared<std::packaged_task<RetType()>>(func);

        std::function<void()> wrapper_func = [task_ptr]() {
            (*task_ptr)();
        };
        task_queue.enqueue(wrapper_func);

        LOG_DEBUG("Task submitted",f,args...);

        return task_ptr->get_future();
    }
};


// // 任务队列
// template <typename T>
// class SafeQueue{
//     public:
//         std::queue<T> s_queue;
//         std::mutex s_mu;
//         std::condition_variable s_cv;
    
//         SafeQueue(){}
//         SafeQueue(SafeQueue &&other){}
//         ~SafeQueue(){}
//         bool empty(){
//             std::lock_guard<std::mutex> lock(s_mu);
//             return s_queue.empty();
//         }

//         int size(){
//             std::lock_guard<std::mutex> lock(s_mu);
//             return s_queue.size();
//         }
//         template <typename U>
//         void enqueue(U &&t){
//             {
//                 std::lock_guard<std::mutex> lock(s_mu);
//                 s_queue.emplace(std::forward<U>(t));
//             }
//             s_cv.notify_one();
//         }

//         bool dequeue(T &t,bool wait=false,bool is_shutdown=false){
//             auto pop_one = [&](T& t){
//                 t = std::move(s_queue.front());
//                 s_queue.pop();
//             };

//             if (wait)
//             {
//                 std::unique_lock<std::mutex> lock(s_mu);
//                 s_cv.wait(lock,[this,is_shutdown]{return is_shutdown||!s_queue.empty();});
//                 if(!s_queue.empty()){
//                     pop_one(t);
//                     return true;
//                 }
//                 return false;
//             }else{
//                 std::lock_guard<std::mutex> lock(s_mu);
//                 if(s_queue.empty()){
//                     return false;
//                 }
//                 pop_one(t);
//                 return true;
//             }
            
//         }
// };

// // 线程池
// class ThreadPool{
//     private:

//     public:
//         // 线程池内置工作线程
//         class ThreadWorker{
//             private:
//                 int w_id;
//                 ThreadPool *w_pool;
//             public:
//                 ThreadWorker(ThreadPool *pool,const int w_id):w_id(w_id), w_pool(pool){}
                
//                 // 重载（）运算符
//                 void operator() (){
//                     std::function<void()> func;
//                     bool dequeued;

//                     while(!w_pool->is_shutdown){
//                         // std::unique_lock<std::mutex> lock(w_pool->pool_mu);
//                         // w_pool->pool_cv.wait(lock,[this]{return !w_pool->task_queue.empty();});
//                         dequeued = w_pool->task_queue.dequeue(func,true,w_pool->is_shutdown);

//                         if(!dequeued) break;
//                             // lock.unlock();
//                         func();
//                         // }
//                     }
//                 }
//         };
//         std::atomic<bool> is_shutdown{false};
//         // std::mutex pool_mu;
//         // std::condition_variable pool_cv;
//         SafeQueue<std::function<void()>> task_queue;
//         std::vector<std::thread> work_thread;

       
//         ThreadPool(const int num_workers):is_shutdown(false),work_thread(std::vector<std::thread>(num_workers)){}
//         // 禁用拷贝和移动
//         ThreadPool(const ThreadPool&) = delete;
//         ThreadPool(ThreadPool&&) = delete;
//         ThreadPool &operator=(const ThreadPool&) = delete;
//         ThreadPool &operator=(ThreadPool&&) = delete;

//         void init(){
//             // 分配工作线程
//             for (std::size_t i=0;i<work_thread.size();i++){
//                 work_thread[i] = std::thread(ThreadWorker(this,i));
//             }
//         }

//         void shutdown(){
//             is_shutdown = true;{   // 唤醒所有在 wait 中阻塞的线程
//             std::unique_lock<std::mutex> lock(task_queue.s_mu);
//             task_queue.s_cv.notify_all();
//             }
//             // pool_cv.notify_all();
//             for(std::size_t i=0;i<work_thread.size();i++){
//                 if(work_thread[i].joinable())
//                     work_thread[i].join();
//             }
//         }

//         // 提交函数执行
//         template <typename F,typename... Args>
//         auto submit(F &&f,Args &&...args)-> std::future<decltype(f(args...))>{

//             // 提交的函数
//             std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f),std::forward<Args>(args)...);

//             auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

//             std::function<void()> warpper_func = [task_ptr](){
//                 (*task_ptr)();
//             };
//             task_queue.enqueue(warpper_func);
            
//             // pool_cv.notify_one();


//             return task_ptr->get_future();
//         }

// };





