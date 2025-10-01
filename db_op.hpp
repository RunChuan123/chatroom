#include<mysql/mysql.h>
#include "debug_logger.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <mutex>
class MySqlDB{
    private:
        MYSQL *conn;
        std::mutex mu;
    public:
        MySqlDB(const std::string& host,
                const std::string& user,
                const std::string& passwd,
                const std::string& db,
                unsigned int port = 3306){

            conn = mysql_init(nullptr);
            if(!conn){
                LOG_ERROR("mysql init failed");
                throw std::runtime_error("mysql init failed");
            }

            if(!mysql_real_connect(conn,host.c_str(),user.c_str(),passwd.c_str(),db.c_str(),port,nullptr,0)){
                LOG_ERROR("mysql connect failed");
                throw std::runtime_error("mysql connect failed");
            }
            LOG_INFO("mysql connect success");
            mysql_set_character_set(conn,"utf8");
        }
        ~MySqlDB(){
            if(conn){
                mysql_close(conn);
                LOG_INFO("mysql closed");
            }
        }
        bool exec(const std::string& sql){
            LOG_DEBUG("mysql query: %s",sql.c_str());

            std::lock_guard<std::mutex> lock(mu);
            if (mysql_query(conn, sql.c_str())) {
                LOG_ERROR("mysql query failed");
                std::cerr << "MySQL query error: " << mysql_error(conn) << std::endl;
                return false;
            }
            return true;
        }

        auto query(const std::string& sql){
            LOG_DEBUG("mysql query: %s",sql.c_str());
            std::lock_guard<std::mutex> lock(mu);
            std::vector<std::vector<std::string>> result;
            if (mysql_query(conn, sql.c_str())) {
                LOG_ERROR("mysql query failed");
                std::cerr << "MySQL query error: " << mysql_error(conn) << std::endl;
                return result;
            }

            MYSQL_RES *res = mysql_store_result(conn);
            if(!res) return result;
            int num_fields = mysql_num_fields(res);
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res))){
                std::vector<std::string> record;
                for(int i=0;i<num_fields;i++){
                    record.emplace_back(row[i]?row[i]:"NULL");
                }
                result.push_back(record);
            }
            LOG_DEBUG("mysql query success");
            mysql_free_result(res);
            return result;
        }
};

