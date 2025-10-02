#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <iostream>
#include <sys/epoll.h>
#include <unordered_map>
#include <fcntl.h>
#include <vector>
#include <string>
#include <debug_logger.hpp>
#include "thread.hpp"

// 抽象类
class INetConn{
    public:
        virtual ~INetConn() {}
        virtual bool send(const std::string& data) = 0;
        virtual std::string recv() = 0;
        virtual int get_fd()const =0;
};


class TcpConn: public INetConn{ 
        int sock_fd;
    public:
        explicit TcpConn(int fd):sock_fd(fd){}
        ~TcpConn(){if (sock_fd > 0) close(sock_fd);}

        bool send(const std::string& data) override{
            int ret = ::send(sock_fd, data.c_str(), data.size(), 0);
            if (ret < 0) {
                std::cout << "send error" << std::endl;
                LOG_ERROR("Tcp send error");
                return false;
            }
            return true;
        }

        std::string recv() override{
            char buf[1024] = {0};
            int ret = ::recv(sock_fd, buf, sizeof(buf), 0);
            if (ret < 0) {
                std::cout << "recv error" << std::endl;
                LOG_ERROR("Tcp recv error");
                return "";
            }
            return std::string(buf,ret);
        }

        int get_fd()const override{
            return sock_fd;
        }

};


class INetServer {
public:
    virtual ~INetServer() {}
    virtual void start(int port) = 0;
    virtual void stop() = 0;
};

class TcpServer : public INetServer {
    int listen_fd;
    bool running;
    int epfd;
    ThreadPool pool;
    std::unordered_map<int,std::unique_ptr<TcpConn>> clients;

public:
    TcpServer() : listen_fd(-1), running(false) {}
    void setNonblocking(int fd){
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    void start(int port) override {
        listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        setNonblocking(listen_fd);
        sockaddr_in serv{};
        serv.sin_family = AF_INET;
        serv.sin_addr.s_addr = INADDR_ANY;
        serv.sin_port = htons(port);

        ::bind(listen_fd, (sockaddr*)&serv, sizeof(serv));
        ::listen(listen_fd, 128);

        epfd = epoll_create(0);
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = listen_fd;

        
        epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);
        pool.init();
        
        running = true;

        LOG_DEBUG("TcpServer start on port %d", port);


        while (running) {
            epoll_event events[EPOLL_MAX_EVENTS];
            int nfds = epoll_wait(epfd,events,EPOLL_MAX_EVENTS, -1);
            for(int i=0;i<nfds;i++){
                int fd = events[i].data.fd;
                // 新客户端
                if(fd == listen_fd){
                    // 也可以绑定IP地址，在user里面有这个字段
                    int client_fd = accept(listen_fd,nullptr,nullptr);
                    setNonblocking(client_fd);
                    clients[client_fd] = std::make_unique<TcpConn>(client_fd);
                    epoll_event cli_ev{};
                    cli_ev.events = EPOLLIN | EPOLLET; //边缘触发
                    cli_ev.data.fd = client_fd;
                    LOG_DEBUG("TcpServer accept new client %d", client_fd);
                    epoll_ctl(epfd,EPOLL_CTL_ADD,client_fd,&cli_ev);
                }
                else if(events[i].events & EPOLLIN){
                    pool.submit([this,fd](){
                        auto it = clients.find(fd);
                        if(it == clients.end()) {
                            LOG_ERROR("TcpServer client %d not found", fd);
                            return;
                        }
                        std::string msg = it->second->recv();
                        if(msg.empty()){
                            LOG_DEBUG("TcpServer client %d disconnect", fd);
                            epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr);
                            clients.erase(it);
                        }else {
                            LOG_DEBUG("TcpServer client %d recv %s", fd, msg.c_str());
                            // 暂时分发给全部
                            for (auto& [other_fd, conn] : clients) {
                                if (other_fd != fd) {
                                    conn->send("User " + std::to_string(fd) + ": " + msg);
                                }
                            }
                        }
                    });
                }
            }
        }
    }

    void stop() override {
        running = false;
        pool.shutdown();
        if(epfd >= 0) close(epfd);
        if (listen_fd >= 0) close(listen_fd);
        LOG_DEBUG("TcpServer stop");
    }
};



class INetClient {
public:
    virtual ~INetClient() {}
    virtual bool connectTo(const std::string& ip, int port) = 0;
    virtual bool send(const std::string& msg) = 0;
    virtual std::string recv() = 0;
};

    
class TcpClient : public INetClient {
    std::unique_ptr<TcpConn> conn;

public:
    bool connectTo(const std::string& ip, int port) override {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in serv{};
        serv.sin_family = AF_INET;
        serv.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &serv.sin_addr);

        if (::connect(fd, (sockaddr*)&serv, sizeof(serv)) < 0) return false;
        LOG_DEBUG("TcpClient connect to %s:%d success", ip.c_str(), port);
        conn = std::make_unique<TcpConn>(fd);
        return true;
    }

    bool send(const std::string& msg) override {
        return conn && conn->send(msg);
    }

    std::string recv() override {
        return conn ? conn->recv() : "";
    }
};
