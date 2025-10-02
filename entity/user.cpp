#include <chrono>
#include <string>

enum class Gender{ Male,Female,Unkonw};

class User{

    public:
        int id;
        std::string name;
        std::string password_hash;
        std::string cur_ip;
        int sock_fd=-1;
        bool is_online;

        int age;
        Gender gender;
        std::string email;
        
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point last_login;

        User(int uid,const std::string& name,const std::string& pwd_hs):id(uid),name(name),password_hash(pwd_hs){}
        User& setId(int uid) { id = uid; return *this; }
        User& setName(const std::string& n) { name = n; return *this; }
        User& setPassword(const std::string& p) { password_hash = p; return *this; }
        User& setEmail(const std::string& e) { email = e; return *this; }

        User& setAge(int a) { age = a; return *this; }

        User& setGender(Gender g) { gender = g; return *this; }

        User& setIp(const std::string& ip) { cur_ip = ip; return *this; }

        User& setOnline(bool o) { is_online = o; return *this; }

        User& setSockFd(int fd) { sock_fd = fd; return *this; }
};