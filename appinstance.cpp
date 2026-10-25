#include "appinstance.h"

#include "logger.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace {
constexpr int kConnectTimeoutMs = 300;   ///< 连接已有实例的超时（避免卡启动）
constexpr int kWriteTimeoutMs   = 300;   ///< 发送请求的超时
const char *const kShowCommand  = "show";
} // namespace

AppInstance::AppInstance(const QString &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
{
}

AppInstance::~AppInstance()
{
    if (m_server) {
        m_server->close();
        QLocalServer::removeServer(m_key);
    }
}

bool AppInstance::tryBecomePrimary()
{
    // 已经有一个实例占了套接字？
    {
        QLocalSocket probe;
        probe.connectToServer(m_key);
        if (probe.waitForConnected(kConnectTimeoutMs)) {
            probe.write(kShowCommand);
            probe.flush();
            probe.waitForBytesWritten(kWriteTimeoutMs);
            probe.disconnectFromServer();
            Logger::info(QStringLiteral("检测到已有实例在运行，已请求其显示窗口，本次启动退出"));
            return false;
        }
    }

    // 清理上一次异常退出可能遗留的套接字文件（仅 Unix 域套接字需要；
    // Windows 命名管道由系统回收，removeServer 是空操作）
    QLocalServer::removeServer(m_key);

    m_server = new QLocalServer(this);
    // 只允许当前用户连接：避免同机器其他用户向我们的窗口发消息
    m_server->setSocketOptions(QLocalServer::UserAccessOption);

    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                const QByteArray data = socket->readAll();
                if (data.contains(kShowCommand)) {
                    Logger::info(QStringLiteral("收到其它实例的显示请求，把窗口带到前台"));
                    emit showRequested();
                }
            });
        }
    });

    if (!m_server->listen(m_key)) {
        // 命名管道不可用（受限容器等）：不阻断启动，只是失去单实例保护
        m_disabled = true;
        Logger::warning(QStringLiteral("单实例通道不可用（%1），程序将继续启动：%2")
                            .arg(m_server->errorString(), m_key));
        delete m_server;
        m_server = nullptr;
        return true;
    }

    m_primary = true;
    Logger::debug(QStringLiteral("单实例通道已建立: %1").arg(m_key));
    return true;
}
