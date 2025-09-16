#include "mcpserver.h"

#include <QDebug>
#include <QHostAddress>

namespace Qt_MCP_Plugin {
namespace Internal {

MCPServer::MCPServer(QObject *parent)
    : QObject(parent)
    , m_serverP(new QTcpServer(this))
    , m_commandsP(new MCPCommands(this))
    , m_port(3001)
{
    connect(m_serverP, &QTcpServer::newConnection,
            this, &MCPServer::handleNewConnection);
}

MCPServer::~MCPServer()
{
    stop();
    delete m_commandsP;
}

bool MCPServer::start(quint16 port)
{
    m_port = port;
    
    // Try to start on the requested port
    if (!m_serverP->listen(QHostAddress::LocalHost, m_port)) {
        qDebug() << "Port" << m_port << "is in use, trying to find an available port...";
        
        // Try ports from 3001 to 3010
        for (quint16 tryPort = 3001; tryPort <= 3010; ++tryPort) {
            if (m_serverP->listen(QHostAddress::LocalHost, tryPort)) {
                m_port = tryPort;
                qDebug() << "MCP Server started on port" << m_port << "(port" << port << "was busy)";
                return true;
            }
        }
        
        qDebug() << "Failed to start MCP server on any port from 3001-3010";
        qDebug() << "Last error:" << m_serverP->errorString();
        return false;
    }
    
    qDebug() << "MCP Server started on port" << m_port;
    return true;
}

void MCPServer::stop()
{
    // Disconnect all clients
    for (auto *client : m_clients) {
        client->disconnectFromHost();
        if (client->state() == QAbstractSocket::ConnectedState) {
            client->waitForDisconnected(3000);
        }
        client->deleteLater();
    }
    m_clients.clear();
    
    if (m_serverP->isListening()) {
        m_serverP->close();
        qDebug() << "MCP Server stopped";
    }
}

bool MCPServer::isRunning() const
{
    return m_serverP && m_serverP->isListening();
}

quint16 MCPServer::getPort() const
{
    return m_port;
}

void MCPServer::handleNewConnection()
{
    QTcpSocket *client = m_serverP->nextPendingConnection();
    if (!client) {
        return;
    }
    
    m_clients.append(client);
    
    connect(client, &QTcpSocket::readyRead,
            this, &MCPServer::handleClientData);
    connect(client, &QTcpSocket::disconnected,
            this, &MCPServer::handleClientDisconnected);
    
    qDebug() << "New MCP client connected:" << client->peerAddress().toString();
}

void MCPServer::handleClientData()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }
    
    QByteArray data = client->readAll();
    QString dataStr = QString::fromUtf8(data);
    
    // Split by newlines to handle multiple JSON-RPC messages
    QStringList messages = dataStr.split('\n', Qt::SkipEmptyParts);
    
    for (const QString &message : messages) {
        if (message.trimmed().isEmpty()) {
            continue;
        }
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
        
        if (error.error != QJsonParseError::NoError) {
            qDebug() << "JSON parse error:" << error.errorString();
            sendResponse(client, createErrorResponse(-32700, "Parse error"));
            continue;
        }
        
        if (!doc.isObject()) {
            qDebug() << "Invalid JSON-RPC message: not an object";
            sendResponse(client, createErrorResponse(-32600, "Invalid Request"));
            continue;
        }
        
        processRequest(client, doc.object());
    }
}

void MCPServer::handleClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (client) {
        m_clients.removeAll(client);
        client->deleteLater();
        qDebug() << "MCP client disconnected";
    }
}

void MCPServer::sendResponse(QTcpSocket *client, const QJsonObject &response)
{
    if (!client || client->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    
    client->write(data);
    client->flush();
}

void MCPServer::processRequest(QTcpSocket *client, const QJsonObject &request)
{
    // Extract method and parameters
    QString method = request.value("method").toString();
    QJsonValue params = request.value("params");
    QJsonValue id = request.value("id");
    
    // Validate JSON-RPC version
    QString jsonrpc = request.value("jsonrpc").toString();
    if (jsonrpc != "2.0") {
        sendResponse(client, createErrorResponse(-32600, "Invalid Request: jsonrpc must be '2.0'", id));
        return;
    }
    
    if (method.isEmpty()) {
        sendResponse(client, createErrorResponse(-32600, "Invalid Request: method is required", id));
        return;
    }
    
    qDebug() << "Processing MCP request:" << method << "with id:" << id;
    
    QJsonValue result;
    QString errorMessage;
    
    // Route the method to appropriate handler
    if (method == "build") {
        bool successB = m_commandsP->build();
        // For long-running operations, provide timeout hints
        QJsonObject buildResult;
        buildResult["success"] = successB;
        int timeout = m_commandsP->getMethodTimeout("build");
        buildResult["message"] = QString("Build started. This operation may take up to %1 seconds.").arg(timeout);
        buildResult["timeoutInfo"] = "Call getMethodMetadata() for expected operation durations";
        result = buildResult;
    }
    else if (method == "debug") {
        QString debugResults = m_commandsP->debug();
        // Add timeout hint to debug response
        QJsonObject debugResult;
        debugResult["output"] = debugResults;
        debugResult["timeoutInfo"] = "Call getMethodMetadata() for expected operation durations";
        result = debugResult;
    }
    else if (method == "getVersion") {
        QJsonObject versionInfo;
        versionInfo["version"] = m_commandsP->getVersion();
        versionInfo["plugin"] = "Qt MCP Plugin";
        versionInfo["note"] = "Some operations may take several minutes. Call getMethodMetadata() for timeout information.";
        result = versionInfo;
    }
    else if (method == "openFile") {
        if (!params.isObject()) {
            errorMessage = "Invalid parameters for openFile";
        } else {
            QString path = params.toObject().value("path").toString();
            bool successB = m_commandsP->openFile(path);
            result = successB;
        }
    }
    else if (method == "listProjects") {
        QStringList projects = m_commandsP->listProjects();
        QJsonArray projectArray;
        for (const QString &project : projects) {
            projectArray.append(project);
        }
        result = projectArray;
    }
    else if (method == "listBuildConfigs") {
        QStringList configs = m_commandsP->listBuildConfigs();
        QJsonArray configArray;
        for (const QString &config : configs) {
            configArray.append(config);
        }
        result = configArray;
    }
    else if (method == "switchToBuildConfig") {
        if (!params.isObject()) {
            errorMessage = "Invalid parameters for switchToBuildConfig";
        } else {
            QString name = params.toObject().value("name").toString();
            bool successB = m_commandsP->switchToBuildConfig(name);
            result = successB;
        }
    }
    else if (method == "quit") {
        bool successB = m_commandsP->quit();
        result = successB;
    }
    else if (method == "getCurrentProject") {
        QString project = m_commandsP->getCurrentProject();
        result = project;
    }
    else if (method == "getCurrentBuildConfig") {
        QString config = m_commandsP->getCurrentBuildConfig();
        result = config;
    }
    else if (method == "runProject") {
        bool successB = m_commandsP->runProject();
        QJsonObject runResult;
        runResult["success"] = successB;
        int timeout = m_commandsP->getMethodTimeout("runProject");
        runResult["message"] = QString("Project run started. This operation may take up to %1 seconds.").arg(timeout);
        runResult["timeoutInfo"] = "Call getMethodMetadata() for expected operation durations";
        result = runResult;
    }
    else if (method == "cleanProject") {
        bool successB = m_commandsP->cleanProject();
        QJsonObject cleanResult;
        cleanResult["success"] = successB;
        int timeout = m_commandsP->getMethodTimeout("cleanProject");
        cleanResult["message"] = QString("Project clean started. This operation may take up to %1 seconds.").arg(timeout);
        cleanResult["timeoutInfo"] = "Call getMethodMetadata() for expected operation durations";
        result = cleanResult;
    }
    else if (method == "listOpenFiles") {
        QStringList files = m_commandsP->listOpenFiles();
        QJsonArray fileArray;
        for (const QString &file : files) {
            fileArray.append(file);
        }
        result = fileArray;
    }
    else if (method == "listSessions") {
        QStringList sessions = m_commandsP->listSessions();
        QJsonArray sessionArray;
        for (const QString &session : sessions) {
            sessionArray.append(session);
        }
        result = sessionArray;
    }
    else if (method == "getCurrentSession") {
        QString session = m_commandsP->getCurrentSession();
        result = session;
    }
    else if (method == "loadSession") {
        if (!params.isObject()) {
            errorMessage = "Invalid parameters for loadSession";
        } else {
            QString sessionName = params.toObject().value("sessionName").toString();
            bool successB = m_commandsP->loadSession(sessionName);
            QJsonObject loadResult;
            loadResult["success"] = successB;
            int timeout = m_commandsP->getMethodTimeout("loadSession");
            loadResult["message"] = QString("Session loading started. This operation may take up to %1 seconds.").arg(timeout);
            loadResult["timeoutInfo"] = "Call getMethodMetadata() for expected operation durations";
            result = loadResult;
        }
    }
    else if (method == "saveSession") {
        bool successB = m_commandsP->saveSession();
        result = successB;
    }
    else if (method == "listIssues") {
        QStringList issues = m_commandsP->listIssues();
        result = QJsonArray::fromStringList(issues);
    }
    else if (method == "listMethods") {
        QJsonArray methods;
        methods.append("build");
        methods.append("debug");
        methods.append("getVersion");
        methods.append("openFile");
        methods.append("listProjects");
        methods.append("listBuildConfigs");
        methods.append("switchToBuildConfig");
        methods.append("quit");
        methods.append("getCurrentProject");
        methods.append("getCurrentBuildConfig");
        methods.append("runProject");
        methods.append("cleanProject");
        methods.append("listOpenFiles");
        methods.append("listSessions");
        methods.append("getCurrentSession");
        methods.append("loadSession");
        methods.append("saveSession");
        methods.append("listIssues");
        methods.append("listMethods");
        methods.append("getMethodMetadata");
        methods.append("setMethodMetadata");
        methods.append("testTaskAccess");
        result = methods;
    }
    else if (method == "getMethodMetadata") {
        QJsonObject metadata;
        
        // Get current timeout values from MCPCommands
        QJsonObject methodDurations;
        QStringList methods = {"debug", "build", "runProject", "loadSession", "cleanProject"};
        for (const QString &methodName : methods) {
            int timeout = m_commandsP->getMethodTimeout(methodName);
            if (timeout >= 0) {
                methodDurations[methodName] = timeout;
            }
        }
        
        metadata["expectedDurations"] = methodDurations;
        metadata["description"] = "Provides metadata about MCP methods, including expected operation durations in seconds";
        metadata["note"] = "Use setMethodMetadata() to customize timeout values";
        
        result = metadata;
    }
    else if (method == "setMethodMetadata") {
        if (!params.isObject()) {
            errorMessage = "Invalid parameters for setMethodMetadata";
        } else {
            QString methodName = params.toObject().value("method").toString();
            int timeoutSeconds = params.toObject().value("timeoutSeconds").toInt();
            QString resultStr = m_commandsP->setMethodMetadata(methodName, timeoutSeconds);
            result = resultStr;
        }
    }
    else if (method == "testTaskAccess") {
        QStringList results = m_commandsP->testTaskAccess();
        result = QJsonArray::fromStringList(results);
    }
    else {
        errorMessage = QString("Unknown method: %1").arg(method);
    }
    
    if (!errorMessage.isEmpty()) {
        sendResponse(client, createErrorResponse(-32601, errorMessage, id));
    } else {
        sendResponse(client, createSuccessResponse(result, id));
    }
}

QJsonObject MCPServer::createErrorResponse(int code, const QString &message, const QJsonValue &id)
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    
    QJsonObject error;
    error["code"] = code;
    error["message"] = message;
    response["error"] = error;
    
    return response;
}

QJsonObject MCPServer::createSuccessResponse(const QJsonValue &result, const QJsonValue &id)
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    
    return response;
}

} // namespace Internal
} // namespace Qt_MCP_Plugin
