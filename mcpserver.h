#ifndef MCPSERVER_H
#define MCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

#include "mcpcommands.h"

namespace Qt_MCP_Plugin {
namespace Internal {

class MCPServer : public QObject
{
    Q_OBJECT

public:
    explicit MCPServer(QObject *parent = nullptr);
    ~MCPServer();

    bool start(quint16 port = 3001);
    void stop();
    bool isRunning() const;
    quint16 getPort() const;

private slots:
    void handleNewConnection();
    void handleClientData();
    void handleClientDisconnected();

private:
    void sendResponse(QTcpSocket *client, const QJsonObject &response);
    void processRequest(QTcpSocket *client, const QJsonObject &request);
    QJsonObject createErrorResponse(int code, const QString &message, const QJsonValue &id = QJsonValue::Null);
    QJsonObject createSuccessResponse(const QJsonValue &result, const QJsonValue &id = QJsonValue::Null);

private:
    QTcpServer *m_serverP;
    QList<QTcpSocket*> m_clients;
    MCPCommands *m_commandsP;
    quint16 m_port;
};

} // namespace Internal
} // namespace Qt_MCP_Plugin

#endif // MCPSERVER_H
