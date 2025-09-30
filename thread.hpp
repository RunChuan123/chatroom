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


// 任务队列
template <typename T>
class SafeQueue{
    public:
        std::queue<T> s_queue;
        std::mutex s_mu;
        std::condition_variable s_cv;
    
        SafeQueue(){}
        SafeQueue(SafeQueue &&other){}
        ~SafeQueue(){}
        bool empty(){
            std::lock_guard<std::mutex> lock(s_mu);
            return s_queue.empty();
        }

        int size(){
            std::lock_guard<std::mutex> lock(s_mu);
            return s_queue.size();
        }
        template <typename U>
        void enqueue(U &&t){
            {
                std::lock_guard<std::mutex> lock(s_mu);
                s_queue.emplace(std::forward<U>(t));
            }
            s_cv.notify_one();
        }

        bool dequeue(T &t,bool wait=false,const std::atomic<bool>& is_shutdown=false){
            auto pop_one = [&](T& t){
                t = std::move(s_queue.front());
                s_queue.pop();
            };

            if (wait)
            {
                std::unique_lock<std::mutex> lock(s_mu);
                s_cv.wait(lock,[this,&is_shutdown]{return is_shutdown||!s_queue.empty();});
                if(!s_queue.empty()){
                    pop_one(t);
                    return true;
                }
                return false;
            }else{
                std::lock_guard<std::mutex> lock(s_mu);
                if(s_queue.empty()){
                    return false;
                }
                pop_one(t);
                return true;
            }
            
        }
};

// 线程池
class ThreadPool{
    private:

    public:
        // 线程池内置工作线程
        class ThreadWorker{
            private:
                int w_id;
                ThreadPool *w_pool;
            public:
                ThreadWorker(ThreadPool *pool,const int w_id):   w_pool(pool),w_id(w_id){}
                
                // 重载（）运算符
                void operator() (){
                    std::function<void()> func;
                    bool dequeued;

                    while(true){
                        // std::unique_lock<std::mutex> lock(w_pool->pool_mu);
                        // w_pool->pool_cv.wait(lock,[this]{return !w_pool->task_queue.empty();});
                        dequeued = w_pool->task_queue.dequeue(func,true,w_pool->is_shutdown);

                        if(!dequeued) break;
                            // lock.unlock();
                        func();
                        // }
                    }
                }
        };
        std::atomic<bool> is_shutdown{false};
        // std::mutex pool_mu;
        // std::condition_variable pool_cv;
        SafeQueue<std::function<void()>> task_queue;
        std::vector<std::thread> work_thread;

       
        ThreadPool(const int num_workers):work_thread(std::vector<std::thread>(num_workers)),is_shutdown(false){}
        // 禁用拷贝和移动
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool &operator=(const ThreadPool&) = delete;
        ThreadPool &operator=(ThreadPool&&) = delete;

        void init(){
            // 分配工作线程
            for (int i=0;i<work_thread.size();i++){
                work_thread[i] = std::thread(ThreadWorker(this,i));
            }
        }

        void shutdown(){
            std::cout << "invoke shutdown" << std::endl;
            is_shutdown = true;
            // 唤醒所有在 wait 中阻塞的线程
            // std::unique_lock<std::mutex> lock(task_queue.s_mu);
            task_queue.s_cv.notify_all();
            std::cout << "shutdown" << std::endl;
            // pool_cv.notify_all();
            for(auto &t: work_thread){
                if(t.joinable())
                    t.join();
            }
            std::cout << "all threads joined" << std::endl;
        }

        // 提交函数执行
        template <typename F,typename... Args>
        auto submit(F &&f,Args &&...args)-> std::future<decltype(f(args...))>{
            // 提交的函数
            std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f),std::forward<Args>(args)...);

            auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

            std::function<void()> warpper_func = [task_ptr](){
                (*task_ptr)();
            };
            task_queue.enqueue(warpper_func);
            
            // pool_cv.notify_one();

            return task_ptr->get_future();
        }

};





