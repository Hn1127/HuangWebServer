#include "sqlConnectionPool.h"

MYSQL *sqlConnectionPool::getConnection()
{
    MYSQL *con = NULL;
    // 从SQL连接池中获取
    m_sem.wait();
    {
        MutexLockGuard mutexLockGuard(m_mutex);
        if (m_maxConn == 0)
        {
            return nullptr;
        }
        con = m_connList.front();
        m_connList.pop_front();
        --m_freeConn;
        ++m_curConn;
    }
    return con;
}

bool sqlConnectionPool::releaseConnection(MYSQL *conn)
{
    if (conn == nullptr)
        return false;
    // 加入SQL连接池
    {
        MutexLockGuard mutexLockGuard(m_mutex);
        if (m_maxConn == 0)
        {
            return false;
        }
        m_connList.push_back(conn);
        ++m_freeConn;
        --m_curConn;
        m_sem.post();
    }
    return true;
}

void sqlConnectionPool::destroyPool()
{
    // 销毁所有的连接
    {
        MutexLockGuard mutexLockGuard(m_mutex);
        for (auto &it : m_connList)
        {
            mysql_close(&(*it));
        }
        m_maxConn = 0;
    }
    m_curConn = 0;
    m_freeConn = 0;
    m_connList.clear();
}

sqlConnectionPool *sqlConnectionPool::getInstance()
{
    static sqlConnectionPool connPool;
    return &connPool;
}

void sqlConnectionPool::init(std::string url, std::string user, std::string password, std::string databaseName, int port, int maxConn)
{
    m_url = url;
    m_user = user;
    m_password = password;
    m_databaseName = databaseName;
    m_port = port;
    m_maxConn = maxConn;
    // 初始化sql连接
    for (int i = 0; i < m_maxConn; i++)
    {
        MYSQL *con = NULL;
        con = mysql_init(con);

        if (con == NULL)
        {
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), user.c_str(), password.c_str(), databaseName.c_str(), port, NULL, 0);

        if (con == NULL)
        {
            exit(1);
        }
        m_connList.push_back(con);
        ++m_freeConn;
    }
    m_maxConn = m_freeConn;
    m_sem = sem(m_freeConn);
}

sqlConnectionPool::sqlConnectionPool()
{
    m_curConn = 0;
    m_freeConn = 0;
}

sqlConnectionPool::~sqlConnectionPool()
{
    destroyPool();
}

sqlConnectionRAII::sqlConnectionRAII(MYSQL *&con, sqlConnectionPool *connPool)
{
    con = connPool->getConnection();
    conRAII = con;
    poolRAII = connPool;
}

sqlConnectionRAII::~sqlConnectionRAII()
{
    poolRAII->releaseConnection(conRAII);
}
