#pragma once
#include <fstream>
#include <iostream>
#include <string>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <thread>
enum class LogLevel{
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger{
    public:
        static Logger& instance(){
            static Logger logger("debug.log");
            return logger;
        }
        template <typename... Args>
        void log(LogLevel level,const std::string& file,int line,const std::string& func,Args&&... args){

             std::ostringstream oss;
            oss << "[" << timestamp() << "]"
            << "[" << levelToString(level) << "]"
            << "[TID=" << std::this_thread::get_id() << "]"
            << "[" << file << ":" << line << "(" << func << ")] ";

            // 逐个参数安全拼接
            ((oss << toString(std::forward<Args>(args)) << " "), ...);

                {
                    std::lock_guard<std::mutex> lock(mu);
                    ofs << oss.str() << std::endl;
                    ofs.flush();
                }
        }
    private:
        std::ofstream ofs;
        std::mutex mu;
        Logger(const std::string& filename){
            ofs.open(filename,std::ios::trunc);
            if(!ofs){
                std::cerr << "can not open log file: " << filename << std::endl;
            }
        }
        ~Logger(){
            ofs.close();
        }
        Logger(const Logger&)=delete;
        Logger& operator=(const Logger&)=delete;

        std::string levelToString(LogLevel level){
            switch(level){
                case LogLevel::DEBUG:
                    return "DEBUG";
                case LogLevel::INFO:
                    return "INFO";
                case LogLevel::WARNING:
                    return "WARNING";
                case LogLevel::ERROR:
                    return "ERROR";
            }
            return "UNKNOWN";
        }
        std::string timestamp(){
            std::time_t t = std::time(nullptr);
            std::tm tm;
            localtime_r(&t,&tm);
            std::ostringstream oss;
            oss << std::put_time(&tm,"%Y-%m-%d %H:%M:%S");
            return oss.str();
        }

        template <typename T>
        static std::string toString(const T& v) {
            std::ostringstream oss;
            if constexpr (std::is_convertible_v<T, std::string>) {
                oss << v;
            } else if constexpr (std::is_arithmetic_v<T>) {
                oss << v;
            } else {
                oss << "[object@" << (const void*)&v << "]";
            }
            return oss.str();
        }
};

#define LOG_DEBUG(...) Logger::instance().log(LogLevel::DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...)  Logger::instance().log(LogLevel::INFO,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...)  Logger::instance().log(LogLevel::WARNING,__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(...) Logger::instance().log(LogLevel::ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
