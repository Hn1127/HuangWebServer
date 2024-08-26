#pragma once

#include "locker.h"
#include <mysql/mysql.h>
#include <string>
#include <list>

class sqlConnectionPool
{
public:
    // 获取一个数据库连接
    MYSQL *getConnection();
    // 释放连接，将conn放入SQL连接列表
    bool releaseConnection(MYSQL *conn);
    // 销毁所有连接
    void destroyPool();

    // 单例模式
    static sqlConnectionPool *getInstance();

    // 初始化连接池
    void init(std::string url, std::string user, std::string password, std::string databaseName, int port, int maxConn);

private:
    sqlConnectionPool();
    ~sqlConnectionPool();

    int m_maxConn;  // 最大连接数
    int m_curConn;  // 当前已使用的连接数
    int m_freeConn; // 当前空闲的连接数

    MutexLock m_mutex;             // 访问SQL连接池的互斥锁
    std::list<MYSQL *> m_connList; // SQL连接列表
    sem m_sem;                     // SQL连接池的信号量

public:
    std::string m_url;          // 主机地址
    std::string m_port;         // 数据库端口号
    std::string m_user;         // 登陆数据库用户名
    std::string m_password;     // 登陆数据库密码
    std::string m_databaseName; // 使用数据库名
};

// 封装一个SQL连接对象，自动进行销毁
class sqlConnectionRAII
{
public:
    sqlConnectionRAII(MYSQL *&con, sqlConnectionPool *connPool);
    ~sqlConnectionRAII();

private:
    MYSQL *conRAII;
    sqlConnectionPool *poolRAII;
};