#include "define.hpp"

#include "db_op.hpp"
#include "thread.hpp"
int main(){
     try {
        // 连接数据库
        MySqlDB db(SQL_IP, SQL_USER, SQL_PASSWD, SQL_DB);

        // 创建一个线程池
        ThreadPool pool(4);
        pool.init();

        // 提交写任务
        pool.submit([&db]() {
            db.exec("INSERT INTO test(id, name) VALUES(11, 'test')");
        });

        // 提交读任务
        pool.submit([&db]() {
            auto rows = db.query("SELECT id, name FROM test");
            for (auto& r : rows) {
                std::cout << "id=" << r[0] << " name=" << r[1] << std::endl;
            }
        });

        pool.shutdown(); // 等待所有任务完成
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}