#include "mcpserver.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QHostAddress>

// Define logging category for MCP server
Q_LOGGING_CATEGORY(mcpServer, "qtcreator.mcpplugin.server", QtWarningMsg)

namespace Qt_MCP_Plugin {
namespace Internal {

MCPServer::MCPServer(QObject *parent)
    : QObject(parent)
    , m_tcpServerP(new QTcpServer(this))
    , m_httpParserP(new HttpParser(this))
    , m_commandsP(new MCPCommands(this))
    , m_port(3001)
{
    // Set up TCP server connections
    connect(m_tcpServerP, &QTcpServer::newConnection, this, &MCPServer::handleNewConnection);
}

MCPServer::~MCPServer()
{
    stop();
    delete m_commandsP;
}

bool MCPServer::isHttpRequest(const QByteArray &data)
{
    return HttpParser::isHttpRequest(data);
}

void MCPServer::handleHttpRequest(QTcpSocket *client, const HttpParser::HttpRequest &request)
{
    qCDebug(mcpServer) << "Handling HTTP request:" << request.method << request.uri;

    // Handle different HTTP methods
    if (request.method == "GET") {
        // Simple GET request - return server info
        QJsonObject serverInfo;
        serverInfo["name"] = "Qt MCP Plugin HTTP Server";
        serverInfo["version"] = "1.0.0";
        serverInfo["transport"] = "HTTP";
        serverInfo["protocol"] = "MCP";
        
        QJsonDocument doc(serverInfo);
        QByteArray response = HttpResponse::createCorsResponse(doc.toJson());
        sendHttpResponse(client, response);
        return;
    }

    if (request.method == "POST") {
        // Handle MCP JSON-RPC requests
        if (request.body.isEmpty()) {
            QByteArray errorResponse = HttpResponse::createErrorResponse(
                HttpResponse::BAD_REQUEST, "Empty request body");
            sendHttpResponse(client, errorResponse);
            return;
        }

        // Parse JSON-RPC request
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(request.body, &error);
        
        if (error.error != QJsonParseError::NoError) {
            qDebug() << "JSON parse error:" << error.errorString();
            QByteArray errorResponse = HttpResponse::createErrorResponse(
                HttpResponse::BAD_REQUEST, "Invalid JSON: " + error.errorString());
            sendHttpResponse(client, errorResponse);
            return;
        }
        
        if (!doc.isObject()) {
            qDebug() << "Invalid JSON-RPC message: not an object";
            QByteArray errorResponse = HttpResponse::createErrorResponse(
                HttpResponse::BAD_REQUEST, "Invalid JSON-RPC: not an object");
            sendHttpResponse(client, errorResponse);
            return;
        }
        
        // Process the MCP request
        QJsonObject response = processRequest(doc.object());
        QByteArray jsonResponse = HttpResponse::createCorsResponse(
            QJsonDocument(response).toJson());
        sendHttpResponse(client, jsonResponse);
        return;
    }

    if (request.method == "OPTIONS") {
        // Handle CORS preflight requests
        QByteArray response = HttpResponse::createCorsResponse(QByteArray(), 
            HttpResponse::NO_CONTENT);
        sendHttpResponse(client, response);
        return;
    }

    // Method not allowed
    QByteArray errorResponse = HttpResponse::createErrorResponse(
        HttpResponse::METHOD_NOT_ALLOWED, "Method not allowed: " + request.method);
    sendHttpResponse(client, errorResponse);
}

void MCPServer::sendHttpResponse(QTcpSocket *client, const QByteArray &httpResponse)
{
    if (!client) return;

    qCDebug(mcpServer) << "Sending HTTP response, size:" << httpResponse.size();
    client->write(httpResponse);
    client->flush();
    
    // Close connection after sending response (HTTP/1.1 behavior)
    client->disconnectFromHost();
}

bool MCPServer::start(quint16 port)
{
    m_port = port;
    qCDebug(mcpServer) << "Starting MCP HTTP server on port" << m_port;
    qCDebug(mcpServer) << "Qt version:" << QT_VERSION_STR;
    qCDebug(mcpServer) << "Build type:" << 
#ifdef QT_NO_DEBUG
        "Release"
#else
        "Debug"
#endif
        ;
    
    // Try to start TCP server on the requested port first
    if (!m_tcpServerP->listen(QHostAddress::LocalHost, m_port)) {
        qCCritical(mcpServer) << "Failed to start MCP TCP server on port" << m_port << ":" << m_tcpServerP->errorString();
        qCWarning(mcpServer) << "Port" << m_port << "is in use, trying to find an available port...";
        
        // Try ports from 3001 to 3010
        for (quint16 tryPort = 3001; tryPort <= 3010; ++tryPort) {
            if (m_tcpServerP->listen(QHostAddress::LocalHost, tryPort)) {
                m_port = tryPort;
                qCInfo(mcpServer) << "MCP TCP Server started on port" << m_port << "(port" << port << "was busy)";
                break;
            }
        }
        
        if (!m_tcpServerP->isListening()) {
            qCCritical(mcpServer) << "Failed to start MCP TCP server on any port from 3001-3010";
            return false;
        }
    }
    
    qCDebug(mcpServer) << "MCP server startup complete - TCP listening:" << m_tcpServerP->isListening();
    
    qCInfo(mcpServer) << "MCP HTTP Server started successfully on port" << m_port;
    return true;
}

void MCPServer::stop()
{
    if (m_tcpServerP && m_tcpServerP->isListening()) {
        m_tcpServerP->close();
        qDebug() << "MCP HTTP Server stopped";
    }
}

bool MCPServer::isRunning() const
{
    return m_tcpServerP && m_tcpServerP->isListening();
}

quint16 MCPServer::getPort() const
{
    return m_port;
}

QJsonObject MCPServer::processRequest(const QJsonObject &request)
{
    // Extract method and parameters
    QString method = request.value("method").toString();
    QJsonValue params = request.value("params");
    QJsonValue id = request.value("id");
    
    // Validate JSON-RPC version
    QString jsonrpc = request.value("jsonrpc").toString();
    if (jsonrpc != "2.0") {
        return createErrorResponse(-32600, "Invalid Request: jsonrpc must be '2.0'", id);
    }
    
    if (method.isEmpty()) {
        return createErrorResponse(-32600, "Invalid Request: method is required", id);
    }
    
    qDebug() << "Processing MCP request:" << method << "with id:" << id;
    
    QJsonValue result;
    QString errorMessage;
    
    // Handle standard MCP protocol methods only
    if (method == "initialize") {
        QJsonObject capabilities;
        QJsonObject toolsCapability;
        toolsCapability["listChanged"] = false;
        capabilities["tools"] = toolsCapability;
        
        QJsonObject serverInfo;
        serverInfo["name"] = "Qt MCP Plugin";
        serverInfo["version"] = m_commandsP->getVersion();
        
        QJsonObject initResult;
        initResult["protocolVersion"] = "2024-11-05";
        initResult["capabilities"] = capabilities;
        initResult["serverInfo"] = serverInfo;
        result = initResult;
    }
    else if (method == "tools/list") {
        QJsonArray tools;
        
        // Build tool
        QJsonObject buildTool;
        buildTool["name"] = "build";
        buildTool["description"] = "Build the current Qt Creator project";
        QJsonObject buildInputSchema;
        buildInputSchema["type"] = "object";
        buildInputSchema["properties"] = QJsonObject();
        buildTool["inputSchema"] = buildInputSchema;
        tools.append(buildTool);
        
        // Get build status tool
        QJsonObject getBuildStatusTool;
        getBuildStatusTool["name"] = "getBuildStatus";
        getBuildStatusTool["description"] = "Get current build progress and status";
        QJsonObject getBuildStatusInputSchema;
        getBuildStatusInputSchema["type"] = "object";
        getBuildStatusInputSchema["properties"] = QJsonObject();
        getBuildStatusTool["inputSchema"] = getBuildStatusInputSchema;
        tools.append(getBuildStatusTool);
        
        // Debug tool
        QJsonObject debugTool;
        debugTool["name"] = "debug";
        debugTool["description"] = "Start debugging the current project";
        QJsonObject debugInputSchema;
        debugInputSchema["type"] = "object";
        debugInputSchema["properties"] = QJsonObject();
        debugTool["inputSchema"] = debugInputSchema;
        tools.append(debugTool);
        
        // Get call stack tool
        QJsonObject getCallStackTool;
        getCallStackTool["name"] = "getCallStack";
        getCallStackTool["description"] = "Get the current call stack when the debugger is paused at a breakpoint. Returns stack frames with function names, file locations, line numbers, and addresses.";
        QJsonObject getCallStackInputSchema;
        getCallStackInputSchema["type"] = "object";
        getCallStackInputSchema["properties"] = QJsonObject();
        getCallStackTool["inputSchema"] = getCallStackInputSchema;
        tools.append(getCallStackTool);
        
        // Open file tool
        QJsonObject openFileTool;
        openFileTool["name"] = "openFile";
        openFileTool["description"] = "Open a file in Qt Creator";
        QJsonObject openFileInputSchema;
        openFileInputSchema["type"] = "object";
        QJsonObject openFileProperties;
        openFileProperties["path"] = QJsonObject{{"type", "string"}, {"description", "Path to the file to open"}};
        openFileInputSchema["properties"] = openFileProperties;
        openFileInputSchema["required"] = QJsonArray{"path"};
        openFileTool["inputSchema"] = openFileInputSchema;
        tools.append(openFileTool);

        // Open preferences panel tool
        QJsonObject openPrefsTool;
        openPrefsTool["name"] = "openPreferencesPanel";
        openPrefsTool["description"] = "Open Preferences/Options dialog and optionally switch to a panel by name or index";
        QJsonObject openPrefsInputSchema;
        openPrefsInputSchema["type"] = "object";
        QJsonObject openPrefsProperties;
        openPrefsProperties["panelName"] = QJsonObject{
            {"type", "string"},
            {"description", "Panel name (partial match) or numeric index as string"}
        };
        openPrefsInputSchema["properties"] = openPrefsProperties;
        openPrefsTool["inputSchema"] = openPrefsInputSchema;
        tools.append(openPrefsTool);
        
        // List projects tool
        QJsonObject listProjectsTool;
        listProjectsTool["name"] = "listProjects";
        listProjectsTool["description"] = "List all available projects";
        QJsonObject listProjectsInputSchema;
        listProjectsInputSchema["type"] = "object";
        listProjectsInputSchema["properties"] = QJsonObject();
        listProjectsTool["inputSchema"] = listProjectsInputSchema;
        tools.append(listProjectsTool);
        
        // List build configs tool
        QJsonObject listBuildConfigsTool;
        listBuildConfigsTool["name"] = "listBuildConfigs";
        listBuildConfigsTool["description"] = "List available build configurations";
        QJsonObject listBuildConfigsInputSchema;
        listBuildConfigsInputSchema["type"] = "object";
        listBuildConfigsInputSchema["properties"] = QJsonObject();
        listBuildConfigsTool["inputSchema"] = listBuildConfigsInputSchema;
        tools.append(listBuildConfigsTool);
        
        // Switch build config tool
        QJsonObject switchBuildConfigTool;
        switchBuildConfigTool["name"] = "switchBuildConfig";
        switchBuildConfigTool["description"] = "Switch to a specific build configuration";
        QJsonObject switchBuildConfigInputSchema;
        switchBuildConfigInputSchema["type"] = "object";
        QJsonObject switchBuildConfigProperties;
        switchBuildConfigProperties["name"] = QJsonObject{{"type", "string"}, {"description", "Name of the build configuration to switch to"}};
        switchBuildConfigInputSchema["properties"] = switchBuildConfigProperties;
        switchBuildConfigInputSchema["required"] = QJsonArray{"name"};
        switchBuildConfigTool["inputSchema"] = switchBuildConfigInputSchema;
        tools.append(switchBuildConfigTool);
        
        // Run project tool
        QJsonObject runProjectTool;
        runProjectTool["name"] = "runProject";
        runProjectTool["description"] = "Run the current project";
        QJsonObject runProjectInputSchema;
        runProjectInputSchema["type"] = "object";
        runProjectInputSchema["properties"] = QJsonObject();
        runProjectTool["inputSchema"] = runProjectInputSchema;
        tools.append(runProjectTool);
        
        // Clean project tool
        QJsonObject cleanProjectTool;
        cleanProjectTool["name"] = "cleanProject";
        cleanProjectTool["description"] = "Clean the current project";
        QJsonObject cleanProjectInputSchema;
        cleanProjectInputSchema["type"] = "object";
        cleanProjectInputSchema["properties"] = QJsonObject();
        cleanProjectTool["inputSchema"] = cleanProjectInputSchema;
        tools.append(cleanProjectTool);
        
        // List open files tool
        QJsonObject listOpenFilesTool;
        listOpenFilesTool["name"] = "listOpenFiles";
        listOpenFilesTool["description"] = "List currently open files";
        QJsonObject listOpenFilesInputSchema;
        listOpenFilesInputSchema["type"] = "object";
        listOpenFilesInputSchema["properties"] = QJsonObject();
        listOpenFilesTool["inputSchema"] = listOpenFilesInputSchema;
        tools.append(listOpenFilesTool);
        
        // List sessions tool
        QJsonObject listSessionsTool;
        listSessionsTool["name"] = "listSessions";
        listSessionsTool["description"] = "List available sessions";
        QJsonObject listSessionsInputSchema;
        listSessionsInputSchema["type"] = "object";
        listSessionsInputSchema["properties"] = QJsonObject();
        listSessionsTool["inputSchema"] = listSessionsInputSchema;
        tools.append(listSessionsTool);
        
        // Load session tool
        QJsonObject loadSessionTool;
        loadSessionTool["name"] = "loadSession";
        loadSessionTool["description"] = "Load a specific session";
        QJsonObject loadSessionInputSchema;
        loadSessionInputSchema["type"] = "object";
        QJsonObject loadSessionProperties;
        loadSessionProperties["sessionName"] = QJsonObject{{"type", "string"}, {"description", "Name of the session to load"}};
        loadSessionInputSchema["properties"] = loadSessionProperties;
        loadSessionInputSchema["required"] = QJsonArray{"sessionName"};
        loadSessionTool["inputSchema"] = loadSessionInputSchema;
        tools.append(loadSessionTool);
        
        // List issues tool
        QJsonObject listIssuesTool;
        listIssuesTool["name"] = "listIssues";
        listIssuesTool["description"] = "List current issues (warnings and errors). Use filter to show only errors or warnings.";
        QJsonObject listIssuesInputSchema;
        listIssuesInputSchema["type"] = "object";
        QJsonObject listIssuesProperties;
        listIssuesProperties["filter"] = QJsonObject{
            {"type", "string"}, 
            {"description", "Filter issues: 'all' (default), 'errors', or 'warnings'"},
            {"enum", QJsonArray{"all", "errors", "warnings"}}
        };
        listIssuesInputSchema["properties"] = listIssuesProperties;
        listIssuesTool["inputSchema"] = listIssuesInputSchema;
        tools.append(listIssuesTool);
        
        // Get build diagnostics (structured JSON for Cursor Problems panel)
        QJsonObject getBuildDiagnosticsTool;
        getBuildDiagnosticsTool["name"] = "getBuildDiagnostics";
        getBuildDiagnosticsTool["description"] = "Get structured build diagnostics as a JSON array (file, line, column, message, severity, code) for use by Cursor Problems panel or problem matchers.";
        QJsonObject getBuildDiagnosticsInputSchema;
        getBuildDiagnosticsInputSchema["type"] = "object";
        QJsonObject getBuildDiagnosticsProperties;
        getBuildDiagnosticsProperties["filter"] = QJsonObject{
            {"type", "string"},
            {"description", "Filter: 'all' (default), 'errors', or 'warnings'"},
            {"enum", QJsonArray{"all", "errors", "warnings"}}
        };
        getBuildDiagnosticsInputSchema["properties"] = getBuildDiagnosticsProperties;
        getBuildDiagnosticsTool["inputSchema"] = getBuildDiagnosticsInputSchema;
        tools.append(getBuildDiagnosticsTool);
        
        // Configure issues tool
        QJsonObject configIssuesTool;
        configIssuesTool["name"] = "configureIssues";
        configIssuesTool["description"] = "Configure issue tracking: set error limit (stop accumulating after N errors) and whether to cancel build on limit";
        QJsonObject configIssuesInputSchema;
        configIssuesInputSchema["type"] = "object";
        QJsonObject configIssuesProperties;
        configIssuesProperties["errorLimit"] = QJsonObject{
            {"type", "integer"}, 
            {"description", "Max errors before stopping accumulation (default 20, 0=unlimited)"}
        };
        configIssuesProperties["stopBuildOnLimit"] = QJsonObject{
            {"type", "boolean"}, 
            {"description", "Cancel build when error limit is reached (default false)"}
        };
        configIssuesInputSchema["properties"] = configIssuesProperties;
        configIssuesTool["inputSchema"] = configIssuesInputSchema;
        tools.append(configIssuesTool);
        
        // Quit tool
        QJsonObject quitTool;
        quitTool["name"] = "quit";
        quitTool["description"] = "Quit Qt Creator";
        QJsonObject quitInputSchema;
        quitInputSchema["type"] = "object";
        quitInputSchema["properties"] = QJsonObject();
        quitTool["inputSchema"] = quitInputSchema;
        tools.append(quitTool);
        
        // Get current project tool
        QJsonObject getCurrentProjectTool;
        getCurrentProjectTool["name"] = "getCurrentProject";
        getCurrentProjectTool["description"] = "Get the currently active project";
        QJsonObject getCurrentProjectInputSchema;
        getCurrentProjectInputSchema["type"] = "object";
        getCurrentProjectInputSchema["properties"] = QJsonObject();
        getCurrentProjectTool["inputSchema"] = getCurrentProjectInputSchema;
        tools.append(getCurrentProjectTool);
        
        // Get current build config tool
        QJsonObject getCurrentBuildConfigTool;
        getCurrentBuildConfigTool["name"] = "getCurrentBuildConfig";
        getCurrentBuildConfigTool["description"] = "Get the currently active build configuration";
        QJsonObject getCurrentBuildConfigInputSchema;
        getCurrentBuildConfigInputSchema["type"] = "object";
        getCurrentBuildConfigInputSchema["properties"] = QJsonObject();
        getCurrentBuildConfigTool["inputSchema"] = getCurrentBuildConfigInputSchema;
        tools.append(getCurrentBuildConfigTool);
        
        // Get current session tool
        QJsonObject getCurrentSessionTool;
        getCurrentSessionTool["name"] = "getCurrentSession";
        getCurrentSessionTool["description"] = "Get the currently active session";
        QJsonObject getCurrentSessionInputSchema;
        getCurrentSessionInputSchema["type"] = "object";
        getCurrentSessionInputSchema["properties"] = QJsonObject();
        getCurrentSessionTool["inputSchema"] = getCurrentSessionInputSchema;
        tools.append(getCurrentSessionTool);
        
        // Save session tool
        QJsonObject saveSessionTool;
        saveSessionTool["name"] = "saveSession";
        saveSessionTool["description"] = "Save the current session";
        QJsonObject saveSessionInputSchema;
        saveSessionInputSchema["type"] = "object";
        saveSessionInputSchema["properties"] = QJsonObject();
        saveSessionTool["inputSchema"] = saveSessionInputSchema;
        tools.append(saveSessionTool);
        
        // Get compile output tool
        QJsonObject getCompileOutputTool;
        getCompileOutputTool["name"] = "getCompileOutput";
        getCompileOutputTool["description"] = "Get the compile output from the build output panel";
        QJsonObject getCompileOutputInputSchema;
        getCompileOutputInputSchema["type"] = "object";
        getCompileOutputInputSchema["properties"] = QJsonObject();
        getCompileOutputTool["inputSchema"] = getCompileOutputInputSchema;
        tools.append(getCompileOutputTool);
        
        // Get application output tool
        QJsonObject getApplicationOutputTool;
        getApplicationOutputTool["name"] = "getApplicationOutput";
        getApplicationOutputTool["description"] = "Get the application output (stdout/stderr) from the running or recently run application";
        QJsonObject getApplicationOutputInputSchema;
        getApplicationOutputInputSchema["type"] = "object";
        getApplicationOutputInputSchema["properties"] = QJsonObject();
        getApplicationOutputTool["inputSchema"] = getApplicationOutputInputSchema;
        tools.append(getApplicationOutputTool);
        
        // Wait for build completion tool
        QJsonObject waitForBuildCompletionTool;
        waitForBuildCompletionTool["name"] = "waitForBuildCompletion";
        waitForBuildCompletionTool["description"] = "Wait for the current build to complete. Blocks until build finishes or timeout.";
        QJsonObject waitForBuildCompletionInputSchema;
        waitForBuildCompletionInputSchema["type"] = "object";
        QJsonObject waitForBuildCompletionProps;
        waitForBuildCompletionProps["timeoutSeconds"] = QJsonObject{
            {"type", "integer"},
            {"description", "Maximum seconds to wait (default 300)"}
        };
        waitForBuildCompletionInputSchema["properties"] = waitForBuildCompletionProps;
        waitForBuildCompletionTool["inputSchema"] = waitForBuildCompletionInputSchema;
        tools.append(waitForBuildCompletionTool);
        
        // listTools: list all tools (including this one and getMethodMetadata)
        QJsonObject listToolsTool;
        listToolsTool["name"] = "listTools";
        listToolsTool["description"] = "List all available MCP tools. Same as the MCP tools/list method. Call this or tools/list to discover every tool including getMethodMetadata and listTools.";
        QJsonObject listToolsInputSchema;
        listToolsInputSchema["type"] = "object";
        listToolsInputSchema["properties"] = QJsonObject();
        listToolsTool["inputSchema"] = listToolsInputSchema;
        tools.append(listToolsTool);
        
        // getMethodMetadata: get timeout values for methods (call this first)
        QJsonObject getMethodMetadataTool;
        getMethodMetadataTool["name"] = "getMethodMetadata";
        getMethodMetadataTool["description"] = "Get method metadata including timeout values for loadSession, build, debug, waitForBuildCompletion, etc. Call this first to discover timeout values.";
        QJsonObject getMethodMetadataInputSchema;
        getMethodMetadataInputSchema["type"] = "object";
        getMethodMetadataInputSchema["properties"] = QJsonObject();
        getMethodMetadataTool["inputSchema"] = getMethodMetadataInputSchema;
        tools.append(getMethodMetadataTool);
        
        // setMethodMetadata: set timeout for a method
        QJsonObject setMethodMetadataTool;
        setMethodMetadataTool["name"] = "setMethodMetadata";
        setMethodMetadataTool["description"] = "Set timeout value (seconds) for a method (e.g. loadSession, build, debug).";
        QJsonObject setMethodMetadataInputSchema;
        setMethodMetadataInputSchema["type"] = "object";
        QJsonObject setMethodMetadataProps;
        setMethodMetadataProps["method"] = QJsonObject{{"type", "string"}, {"description", "Method name (e.g. loadSession, build)"}};
        setMethodMetadataProps["timeoutSeconds"] = QJsonObject{{"type", "integer"}, {"description", "Timeout in seconds"}};
        setMethodMetadataInputSchema["properties"] = setMethodMetadataProps;
        setMethodMetadataTool["inputSchema"] = setMethodMetadataInputSchema;
        tools.append(setMethodMetadataTool);
        
        QJsonObject toolsResult;
        toolsResult["tools"] = tools;
        result = toolsResult;
    }
    else if (method == "tools/call") {
        if (!params.isObject()) {
            errorMessage = "Invalid parameters for tools/call";
        } else {
            QJsonObject paramsObj = params.toObject();
            QString toolName = paramsObj.value("name").toString();
            QJsonValue arguments = paramsObj.value("arguments");
            
            // Helper function to create MCP content format response
            auto createContentResponse = [](const QJsonValue &data) -> QJsonObject {
                QJsonObject contentItem;
                contentItem["type"] = "text";
                
                // Convert data to JSON string for text content
                if (data.isObject()) {
                    QJsonDocument doc(data.toObject());
                    contentItem["text"] = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
                } else if (data.isArray()) {
                    QJsonDocument doc(data.toArray());
                    contentItem["text"] = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
                } else if (data.isString()) {
                    contentItem["text"] = data.toString();
                } else if (data.isBool()) {
                    contentItem["text"] = data.toBool() ? "true" : "false";
                } else {
                    contentItem["text"] = data.toVariant().toString();
                }
                
                QJsonArray contentArray;
                contentArray.append(contentItem);
                
                QJsonObject result;
                result["content"] = contentArray;
                return result;
            };
            
            // Route to appropriate command based on tool name
            if (toolName == "build") {
                bool successB = m_commandsP->build();
                QJsonObject data{{"success", successB}};
                result = createContentResponse(data);
            }
            else if (toolName == "getBuildStatus") {
                QString buildStatusResult = m_commandsP->getBuildStatus();
                QJsonObject data{{"result", buildStatusResult}};
                result = createContentResponse(data);
            }
            else if (toolName == "debug") {
                QString debugResult = m_commandsP->debug();
                QJsonObject data{{"result", debugResult}};
                result = createContentResponse(data);
            }
            else if (toolName == "getCallStack") {
                QString callStackResult = m_commandsP->getCallStack();
                QJsonObject data{{"result", callStackResult}};
                result = createContentResponse(data);
            }
            else if (toolName == "openFile") {
                if (arguments.isObject()) {
                    QString path = arguments.toObject().value("path").toString();
                    bool successB = m_commandsP->openFile(path);
                    QJsonObject data{{"success", successB}};
                    result = createContentResponse(data);
                } else {
                    errorMessage = "Invalid arguments for openFile";
                }
            }
            else if (toolName == "openPreferencesPanel") {
                QString panelName;
                if (arguments.isObject()) {
                    panelName = arguments.toObject().value("panelName").toString();
                }
                bool successB = m_commandsP->openPreferencesPanel(panelName);
                QJsonObject data{{"success", successB}};
                result = createContentResponse(data);
            }
            else if (toolName == "listProjects") {
                QStringList projects = m_commandsP->listProjects();
                QJsonArray projectArray;
                for (const QString &project : projects) {
                    projectArray.append(project);
                }
                QJsonObject data{{"projects", projectArray}};
                result = createContentResponse(data);
            }
            else if (toolName == "listBuildConfigs") {
                QStringList configs = m_commandsP->listBuildConfigs();
                QJsonArray configArray;
                for (const QString &config : configs) {
                    configArray.append(config);
                }
                QJsonObject data{{"buildConfigs", configArray}};
                result = createContentResponse(data);
            }
            else if (toolName == "switchBuildConfig") {
                if (arguments.isObject()) {
                    QString name = arguments.toObject().value("name").toString();
                    bool successB = m_commandsP->switchToBuildConfig(name);
                    QJsonObject data{{"success", successB}};
                    result = createContentResponse(data);
                } else {
                    errorMessage = "Invalid arguments for switchBuildConfig";
                }
            }
            else if (toolName == "runProject") {
                bool successB = m_commandsP->runProject();
                QJsonObject data{{"success", successB}};
                result = createContentResponse(data);
            }
            else if (toolName == "cleanProject") {
                bool successB = m_commandsP->cleanProject();
                QJsonObject data{{"success", successB}};
                result = createContentResponse(data);
            }
            else if (toolName == "listOpenFiles") {
                QStringList files = m_commandsP->listOpenFiles();
                QJsonArray fileArray;
                for (const QString &file : files) {
                    fileArray.append(file);
                }
                QJsonObject data{{"openFiles", fileArray}};
                result = createContentResponse(data);
            }
            else if (toolName == "listSessions") {
                QStringList sessions = m_commandsP->listSessions();
                QJsonArray sessionArray;
                for (const QString &session : sessions) {
                    sessionArray.append(session);
                }
                QJsonObject data{{"sessions", sessionArray}};
                result = createContentResponse(data);
            }
            else if (toolName == "loadSession") {
                if (arguments.isObject()) {
                    QString sessionName = arguments.toObject().value("sessionName").toString();
                    bool successB = m_commandsP->loadSession(sessionName);
                    QJsonObject data{{"success", successB}};
                    result = createContentResponse(data);
                } else {
                    errorMessage = "Invalid arguments for loadSession";
                }
            }
            else if (toolName == "listIssues") {
                QString filter = "all";  // default
                if (arguments.isObject()) {
                    filter = arguments.toObject().value("filter").toString("all");
                }
                QStringList issues = m_commandsP->listIssues(filter);
                QJsonArray issueArray;
                for (const QString &issue : issues) {
                    issueArray.append(issue);
                }
                QJsonObject data{{"issues", issueArray}};
                result = createContentResponse(data);
            }
            else if (toolName == "getBuildDiagnostics") {
                QString filter = "all";
                if (arguments.isObject()) {
                    filter = arguments.toObject().value("filter").toString("all");
                }
                QString jsonStr = m_commandsP->getBuildDiagnostics(filter);
                QJsonArray arr = QJsonDocument::fromJson(jsonStr.toUtf8()).array();
                QJsonObject data{{"diagnostics", arr}};
                result = createContentResponse(data);
            }
            else if (toolName == "configureIssues") {
                if (arguments.isObject()) {
                    QJsonObject args = arguments.toObject();
                    if (args.contains("errorLimit")) {
                        m_commandsP->setErrorLimit(args.value("errorLimit").toInt(20));
                    }
                    if (args.contains("stopBuildOnLimit")) {
                        m_commandsP->setStopBuildOnLimit(args.value("stopBuildOnLimit").toBool(false));
                    }
                }
                QJsonObject data{
                    {"errorLimit", m_commandsP->errorLimit()},
                    {"stopBuildOnLimit", m_commandsP->stopBuildOnLimit()}
                };
                result = createContentResponse(data);
            }
            else if (toolName == "quit") {
                bool successB = m_commandsP->quit();
                QJsonObject data{{"success", successB}};
                result = createContentResponse(data);
            }
            else if (toolName == "getCurrentProject") {
                QString project = m_commandsP->getCurrentProject();
                QJsonObject data{{"project", project}};
                result = createContentResponse(data);
            }
            else if (toolName == "getCurrentBuildConfig") {
                QString config = m_commandsP->getCurrentBuildConfig();
                QJsonObject data{{"buildConfig", config}};
                result = createContentResponse(data);
            }
            else if (toolName == "getCurrentSession") {
                QString session = m_commandsP->getCurrentSession();
                QJsonObject data{{"session", session}};
                result = createContentResponse(data);
            }
            else if (toolName == "saveSession") {
                bool successB = m_commandsP->saveSession();
                QJsonObject data{{"success", successB}};
                result = createContentResponse(data);
            }
            else if (toolName == "getCompileOutput") {
                QString compileOutput = m_commandsP->getCompileOutput();
                QJsonObject data{{"output", compileOutput}};
                result = createContentResponse(data);
            }
            else if (toolName == "getApplicationOutput") {
                QString appOutput = m_commandsP->getApplicationOutput();
                QJsonObject data{{"output", appOutput}};
                result = createContentResponse(data);
            }
            else if (toolName == "waitForBuildCompletion") {
                int timeout = 300; // default 5 minutes
                if (arguments.isObject()) {
                    timeout = arguments.toObject().value("timeoutSeconds").toInt(300);
                }
                bool completed = m_commandsP->waitForBuildCompletion(timeout);
                QJsonObject data{
                    {"completed", completed},
                    {"timedOut", !completed}
                };
                result = createContentResponse(data);
            }
            else if (toolName == "getMethodMetadata") {
                QString meta = m_commandsP->getMethodMetadata();
                QJsonObject data{{"result", meta}};
                result = createContentResponse(data);
            }
            else if (toolName == "setMethodMetadata") {
                QString method;
                int timeoutSeconds = -1;
                if (arguments.isObject()) {
                    QJsonObject args = arguments.toObject();
                    method = args.value("method").toString();
                    timeoutSeconds = args.value("timeoutSeconds").toInt(-1);
                }
                QString setResult = m_commandsP->setMethodMetadata(method, timeoutSeconds);
                QJsonObject data{{"result", setResult}};
                result = createContentResponse(data);
            }
            else if (toolName == "listTools") {
                QJsonObject data{{"message", "Use the MCP method tools/list to get the full list of tools (including listTools and getMethodMetadata)."}};
                result = createContentResponse(data);
            }
            else {
                // Provide helpful suggestions for common typos
                QString suggestion = "";
                if (toolName == "setBuildConfiguration") {
                    suggestion = " (did you mean 'switchBuildConfig'?)";
                } else if (toolName == "setBuildConfig") {
                    suggestion = " (did you mean 'switchBuildConfig'?)";
                } else if (toolName == "switchBuildConfiguration") {
                    suggestion = " (did you mean 'switchBuildConfig'?)";
                } else if (toolName == "getVersion") {
                    suggestion = " (use 'tools/list' to see available tools)";
                }
                errorMessage = "Unknown tool: " + toolName + suggestion;
            }
        }
    }
    else if (method == "prompts/list") {
        // Return empty prompts list to satisfy clients expecting prompt discovery.
        QJsonArray prompts;
        result = QJsonObject{{"prompts", prompts}};
    }
    else {
        errorMessage = QString("Unknown method: %1").arg(method);
    }
    
    if (!errorMessage.isEmpty()) {
        return createErrorResponse(-32601, errorMessage, id);
    } else {
        return createSuccessResponse(result, id);
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

QJsonObject MCPServer::callMCPMethod(const QString &method, const QJsonValue &params)
{
    // Create a fake request object to reuse the existing processRequest logic
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["method"] = method;
    request["id"] = 1;
    
    if (!params.isUndefined()) {
        request["params"] = params;
    }
    
    // Create a fake response object to capture the result
    QJsonObject response;
    response["id"] = 1;
    
    // Process the request (this will populate the response)
    QString errorMessage;
    QJsonValue result;
    
    // Handle standard MCP protocol methods
    if (method == "initialize") {
        QJsonObject capabilities;
        QJsonObject toolsCapability;
        toolsCapability["listChanged"] = false;
        capabilities["tools"] = toolsCapability;
        
        QJsonObject serverInfo;
        serverInfo["name"] = "Qt MCP Plugin";
        serverInfo["version"] = m_commandsP->getVersion();
        
        QJsonObject initResult;
        initResult["protocolVersion"] = "2024-11-05";
        initResult["capabilities"] = capabilities;
        initResult["serverInfo"] = serverInfo;
        result = initResult;
    }
    else if (method == "tools/list") {
        QJsonArray tools;
        
        // Build tool
        QJsonObject buildTool;
        buildTool["name"] = "build";
        buildTool["description"] = "Build the current Qt Creator project";
        QJsonObject buildInputSchema;
        buildInputSchema["type"] = "object";
        buildInputSchema["properties"] = QJsonObject();
        buildTool["inputSchema"] = buildInputSchema;
        tools.append(buildTool);
        
        // Get build status tool
        QJsonObject getBuildStatusTool;
        getBuildStatusTool["name"] = "getBuildStatus";
        getBuildStatusTool["description"] = "Get current build progress and status";
        QJsonObject getBuildStatusInputSchema;
        getBuildStatusInputSchema["type"] = "object";
        getBuildStatusInputSchema["properties"] = QJsonObject();
        getBuildStatusTool["inputSchema"] = getBuildStatusInputSchema;
        tools.append(getBuildStatusTool);
        
        // Debug tool
        QJsonObject debugTool;
        debugTool["name"] = "debug";
        debugTool["description"] = "Start debugging the current project";
        QJsonObject debugInputSchema;
        debugInputSchema["type"] = "object";
        debugInputSchema["properties"] = QJsonObject();
        debugTool["inputSchema"] = debugInputSchema;
        tools.append(debugTool);
        
        // Get call stack tool
        QJsonObject getCallStackTool;
        getCallStackTool["name"] = "getCallStack";
        getCallStackTool["description"] = "Get the current call stack when the debugger is paused at a breakpoint. Returns stack frames with function names, file locations, line numbers, and addresses.";
        QJsonObject getCallStackInputSchema;
        getCallStackInputSchema["type"] = "object";
        getCallStackInputSchema["properties"] = QJsonObject();
        getCallStackTool["inputSchema"] = getCallStackInputSchema;
        tools.append(getCallStackTool);
        
        // Open file tool
        QJsonObject openFileTool;
        openFileTool["name"] = "openFile";
        openFileTool["description"] = "Open a file in Qt Creator";
        QJsonObject openFileInputSchema;
        openFileInputSchema["type"] = "object";
        QJsonObject openFileProperties;
        openFileProperties["path"] = QJsonObject{{"type", "string"}, {"description", "Path to the file to open"}};
        openFileInputSchema["properties"] = openFileProperties;
        openFileInputSchema["required"] = QJsonArray{"path"};
        openFileTool["inputSchema"] = openFileInputSchema;
        tools.append(openFileTool);

        // Open preferences panel tool
        QJsonObject openPrefsTool;
        openPrefsTool["name"] = "openPreferencesPanel";
        openPrefsTool["description"] = "Open Preferences/Options dialog and optionally switch to a panel by name or index";
        QJsonObject openPrefsInputSchema;
        openPrefsInputSchema["type"] = "object";
        QJsonObject openPrefsProperties;
        openPrefsProperties["panelName"] = QJsonObject{
            {"type", "string"},
            {"description", "Panel name (partial match) or numeric index as string"}
        };
        openPrefsInputSchema["properties"] = openPrefsProperties;
        openPrefsTool["inputSchema"] = openPrefsInputSchema;
        tools.append(openPrefsTool);
        
        // List projects tool
        QJsonObject listProjectsTool;
        listProjectsTool["name"] = "listProjects";
        listProjectsTool["description"] = "List all available projects";
        QJsonObject listProjectsInputSchema;
        listProjectsInputSchema["type"] = "object";
        listProjectsInputSchema["properties"] = QJsonObject();
        listProjectsTool["inputSchema"] = listProjectsInputSchema;
        tools.append(listProjectsTool);
        
        // List build configs tool
        QJsonObject listBuildConfigsTool;
        listBuildConfigsTool["name"] = "listBuildConfigs";
        listBuildConfigsTool["description"] = "List available build configurations";
        QJsonObject listBuildConfigsInputSchema;
        listBuildConfigsInputSchema["type"] = "object";
        listBuildConfigsInputSchema["properties"] = QJsonObject();
        listBuildConfigsTool["inputSchema"] = listBuildConfigsInputSchema;
        tools.append(listBuildConfigsTool);
        
        // Switch build config tool
        QJsonObject switchBuildConfigTool;
        switchBuildConfigTool["name"] = "switchBuildConfig";
        switchBuildConfigTool["description"] = "Switch to a specific build configuration";
        QJsonObject switchBuildConfigInputSchema;
        switchBuildConfigInputSchema["type"] = "object";
        QJsonObject switchBuildConfigProperties;
        switchBuildConfigProperties["name"] = QJsonObject{{"type", "string"}, {"description", "Name of the build configuration to switch to"}};
        switchBuildConfigInputSchema["properties"] = switchBuildConfigProperties;
        switchBuildConfigInputSchema["required"] = QJsonArray{"name"};
        switchBuildConfigTool["inputSchema"] = switchBuildConfigInputSchema;
        tools.append(switchBuildConfigTool);
        
        // Run project tool
        QJsonObject runProjectTool;
        runProjectTool["name"] = "runProject";
        runProjectTool["description"] = "Run the current project";
        QJsonObject runProjectInputSchema;
        runProjectInputSchema["type"] = "object";
        runProjectInputSchema["properties"] = QJsonObject();
        runProjectTool["inputSchema"] = runProjectInputSchema;
        tools.append(runProjectTool);
        
        // Clean project tool
        QJsonObject cleanProjectTool;
        cleanProjectTool["name"] = "cleanProject";
        cleanProjectTool["description"] = "Clean the current project";
        QJsonObject cleanProjectInputSchema;
        cleanProjectInputSchema["type"] = "object";
        cleanProjectInputSchema["properties"] = QJsonObject();
        cleanProjectTool["inputSchema"] = cleanProjectInputSchema;
        tools.append(cleanProjectTool);
        
        // List open files tool
        QJsonObject listOpenFilesTool;
        listOpenFilesTool["name"] = "listOpenFiles";
        listOpenFilesTool["description"] = "List currently open files";
        QJsonObject listOpenFilesInputSchema;
        listOpenFilesInputSchema["type"] = "object";
        listOpenFilesInputSchema["properties"] = QJsonObject();
        listOpenFilesTool["inputSchema"] = listOpenFilesInputSchema;
        tools.append(listOpenFilesTool);
        
        // List sessions tool
        QJsonObject listSessionsTool;
        listSessionsTool["name"] = "listSessions";
        listSessionsTool["description"] = "List available sessions";
        QJsonObject listSessionsInputSchema;
        listSessionsInputSchema["type"] = "object";
        listSessionsInputSchema["properties"] = QJsonObject();
        listSessionsTool["inputSchema"] = listSessionsInputSchema;
        tools.append(listSessionsTool);
        
        // Load session tool
        QJsonObject loadSessionTool;
        loadSessionTool["name"] = "loadSession";
        loadSessionTool["description"] = "Load a specific session";
        QJsonObject loadSessionInputSchema;
        loadSessionInputSchema["type"] = "object";
        QJsonObject loadSessionProperties;
        loadSessionProperties["sessionName"] = QJsonObject{{"type", "string"}, {"description", "Name of the session to load"}};
        loadSessionInputSchema["properties"] = loadSessionProperties;
        loadSessionInputSchema["required"] = QJsonArray{"sessionName"};
        loadSessionTool["inputSchema"] = loadSessionInputSchema;
        tools.append(loadSessionTool);
        
        // List issues tool
        QJsonObject listIssuesTool;
        listIssuesTool["name"] = "listIssues";
        listIssuesTool["description"] = "List current issues (warnings and errors). Use filter to show only errors or warnings.";
        QJsonObject listIssuesInputSchema;
        listIssuesInputSchema["type"] = "object";
        QJsonObject listIssuesProperties;
        listIssuesProperties["filter"] = QJsonObject{
            {"type", "string"}, 
            {"description", "Filter issues: 'all' (default), 'errors', or 'warnings'"},
            {"enum", QJsonArray{"all", "errors", "warnings"}}
        };
        listIssuesInputSchema["properties"] = listIssuesProperties;
        listIssuesTool["inputSchema"] = listIssuesInputSchema;
        tools.append(listIssuesTool);
        
        // Get build diagnostics (structured JSON for Cursor Problems panel)
        QJsonObject getBuildDiagnosticsTool;
        getBuildDiagnosticsTool["name"] = "getBuildDiagnostics";
        getBuildDiagnosticsTool["description"] = "Get structured build diagnostics as a JSON array (file, line, column, message, severity, code) for use by Cursor Problems panel or problem matchers.";
        QJsonObject getBuildDiagnosticsInputSchema;
        getBuildDiagnosticsInputSchema["type"] = "object";
        QJsonObject getBuildDiagnosticsProperties;
        getBuildDiagnosticsProperties["filter"] = QJsonObject{
            {"type", "string"},
            {"description", "Filter: 'all' (default), 'errors', or 'warnings'"},
            {"enum", QJsonArray{"all", "errors", "warnings"}}
        };
        getBuildDiagnosticsInputSchema["properties"] = getBuildDiagnosticsProperties;
        getBuildDiagnosticsTool["inputSchema"] = getBuildDiagnosticsInputSchema;
        tools.append(getBuildDiagnosticsTool);
        
        
        // Configure issues tool
        QJsonObject configIssuesTool;
        configIssuesTool["name"] = "configureIssues";
        configIssuesTool["description"] = "Configure issue tracking: set error limit (stop accumulating after N errors) and whether to cancel build on limit";
        QJsonObject configIssuesInputSchema;
        configIssuesInputSchema["type"] = "object";
        QJsonObject configIssuesProperties;
        configIssuesProperties["errorLimit"] = QJsonObject{
            {"type", "integer"}, 
            {"description", "Max errors before stopping accumulation (default 20, 0=unlimited)"}
        };
        configIssuesProperties["stopBuildOnLimit"] = QJsonObject{
            {"type", "boolean"}, 
            {"description", "Cancel build when error limit is reached (default false)"}
        };
        configIssuesInputSchema["properties"] = configIssuesProperties;
        configIssuesTool["inputSchema"] = configIssuesInputSchema;
        tools.append(configIssuesTool);
        
        // Quit tool
        QJsonObject quitTool;
        quitTool["name"] = "quit";
        quitTool["description"] = "Quit Qt Creator";
        QJsonObject quitInputSchema;
        quitInputSchema["type"] = "object";
        quitInputSchema["properties"] = QJsonObject();
        quitTool["inputSchema"] = quitInputSchema;
        tools.append(quitTool);
        
        // listTools
        QJsonObject listToolsTool2;
        listToolsTool2["name"] = "listTools";
        listToolsTool2["description"] = "List all available MCP tools. Same as the MCP tools/list method. Call this or tools/list to discover every tool including getMethodMetadata and listTools.";
        QJsonObject listToolsInputSchema2;
        listToolsInputSchema2["type"] = "object";
        listToolsInputSchema2["properties"] = QJsonObject();
        listToolsTool2["inputSchema"] = listToolsInputSchema2;
        tools.append(listToolsTool2);
        
        // getMethodMetadata
        QJsonObject getMethodMetadataTool2;
        getMethodMetadataTool2["name"] = "getMethodMetadata";
        getMethodMetadataTool2["description"] = "Get method metadata including timeout values for loadSession, build, debug, waitForBuildCompletion, etc. Call this first to discover timeout values.";
        QJsonObject getMethodMetadataInputSchema2;
        getMethodMetadataInputSchema2["type"] = "object";
        getMethodMetadataInputSchema2["properties"] = QJsonObject();
        getMethodMetadataTool2["inputSchema"] = getMethodMetadataInputSchema2;
        tools.append(getMethodMetadataTool2);
        
        // setMethodMetadata
        QJsonObject setMethodMetadataTool2;
        setMethodMetadataTool2["name"] = "setMethodMetadata";
        setMethodMetadataTool2["description"] = "Set timeout value (seconds) for a method (e.g. loadSession, build, debug).";
        QJsonObject setMethodMetadataInputSchema2;
        setMethodMetadataInputSchema2["type"] = "object";
        QJsonObject setMethodMetadataProps2;
        setMethodMetadataProps2["method"] = QJsonObject{{"type", "string"}, {"description", "Method name (e.g. loadSession, build)"}};
        setMethodMetadataProps2["timeoutSeconds"] = QJsonObject{{"type", "integer"}, {"description", "Timeout in seconds"}};
        setMethodMetadataInputSchema2["properties"] = setMethodMetadataProps2;
        setMethodMetadataTool2["inputSchema"] = setMethodMetadataInputSchema2;
        tools.append(setMethodMetadataTool2);
        
        QJsonObject toolsResult;
        toolsResult["tools"] = tools;
        result = toolsResult;
    }
    else {
        // Unknown method
        response["jsonrpc"] = "2.0";
        response["id"] = 1;
        QJsonObject error;
        error["code"] = -32601;
        error["message"] = QJsonValue(QString("Unknown method: ") + method);
        response["error"] = error;
        return response;
    }
    
    // Create success response
    response["jsonrpc"] = "2.0";
    response["id"] = 1;
    response["result"] = result;
    
    return response;
}

void MCPServer::handleNewConnection()
{
    QTcpSocket *client = m_tcpServerP->nextPendingConnection();
    if (!client) return;
    
    m_clients.append(client);
    connect(client, &QTcpSocket::readyRead, this, &MCPServer::handleClientData);
    connect(client, &QTcpSocket::disconnected, this, &MCPServer::handleClientDisconnected);
    
    qCDebug(mcpServer) << "New TCP client connected, total clients:" << m_clients.size();
}

void MCPServer::handleClientData()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    QByteArray data = client->readAll();
    qCDebug(mcpServer) << "Received data, size:" << data.size();

    // Check if this is an HTTP request
    if (isHttpRequest(data)) {
        qCDebug(mcpServer) << "Detected HTTP request, parsing...";
        
        // Parse HTTP request
        HttpParser::HttpRequest httpRequest = m_httpParserP->parseRequest(data);
        if (!httpRequest.isValid) {
            qDebug() << "Invalid HTTP request:" << httpRequest.errorMessage;
            QByteArray errorResponse = HttpResponse::createErrorResponse(
                HttpResponse::BAD_REQUEST, httpRequest.errorMessage);
            sendHttpResponse(client, errorResponse);
            return;
        }
        
        // Handle HTTP request
        handleHttpRequest(client, httpRequest);
        return;
    }

    // Handle as TCP/JSON-RPC request (original behavior)
    qCDebug(mcpServer) << "Handling as TCP/JSON-RPC request";
    
    // Parse JSON-RPC request
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << error.errorString();
        QJsonObject errorResponse = createErrorResponse(-32700, "Parse error");
        sendResponse(client, errorResponse);
        return;
    }

    if (!doc.isObject()) {
        qDebug() << "Invalid JSON-RPC message: not an object";
        QJsonObject errorResponse = createErrorResponse(-32600, "Invalid Request");
        sendResponse(client, errorResponse);
        return;
    }

    // Process the MCP request
    QJsonObject response = processRequest(doc.object());
    sendResponse(client, response);
}

void MCPServer::handleClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;
    
    m_clients.removeAll(client);
    client->deleteLater();
    
    qCDebug(mcpServer) << "TCP client disconnected, remaining clients:" << m_clients.size();
}

void MCPServer::sendResponse(QTcpSocket *client, const QJsonObject &response)
{
    if (!client) return;
    
    QJsonDocument doc(response);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    
    qCDebug(mcpServer) << "Sending TCP response:" << data;
    client->write(data);
    client->flush();
}

} // namespace Internal
} // namespace Qt_MCP_Plugin
