#ifndef MCPCOMMANDS_H
#define MCPCOMMANDS_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>

// Forward declarations
namespace Qt_MCP_Plugin {
namespace Internal {
class IssuesManager;
}
}

namespace Qt_MCP_Plugin {
namespace Internal {

class MCPCommands : public QObject
{
    Q_OBJECT

public:
    explicit MCPCommands(QObject *parent = nullptr);

    // Core MCP commands
    bool build();
    QString debug();
    QString stopDebug();
    bool openFile(const QString &path);
    QStringList listProjects();
    QStringList listBuildConfigs();
    bool switchToBuildConfig(const QString &name);
    bool quit();
    QString getVersion();
    QString getBuildStatus();

    // Additional useful commands
    QString getCurrentProject();
    QString getCurrentBuildConfig();
    bool runProject();
    bool cleanProject();
    QStringList listOpenFiles();
    
    // Session management commands
    QStringList listSessions();
    QString getCurrentSession();
    bool loadSession(const QString &sessionName);
    bool saveSession();
    
    // Issue management commands
    QStringList listIssues();
    
    // Build output commands
    QString getCompileOutput();
    
    // Build monitoring
    bool waitForBuildCompletion(int timeoutSeconds = 300);
    bool isBuildInProgress() const;
    
    // Method metadata management
    QString getMethodMetadata();
    QString setMethodMetadata(const QString &method, int timeoutSeconds);
    int getMethodTimeout(const QString &method) const;
    
    // Debugging management helpers
    bool isDebuggingActive();
    QString abortDebug();
    bool killDebuggedProcesses();
    void performDebuggingCleanup();
    bool performDebuggingCleanupSync();

    // === NEW MCP COMMANDS ===

    // Project management
    bool openProject(const QString &path);
    bool runQmake();
    bool rebuild();

    // Debugger control
    bool stepOver();
    bool stepInto();
    bool stepOut();
    bool continueExecution();
    bool pauseExecution();
    bool runToLine(const QString &file, int line);

    // Breakpoint management
    bool setBreakpoint(const QString &file, int line, const QString &condition = QString());
    bool removeBreakpoint(const QString &file, int line);
    bool toggleBreakpoint(const QString &file, int line);
    bool removeAllBreakpoints();
    QJsonArray listBreakpoints();

    // Stack and variables
    QJsonArray getStackTrace();
    QJsonObject getVariables(const QString &scope = QString());
    QString evaluateExpression(const QString &expression);

    // Debugger state
    QString getDebuggerState();
    bool isDebuggerRunning() const;
    bool isDebuggerPaused() const;

signals:
    void sessionLoadRequested(const QString &sessionName);
    void buildStateChanged();

private slots:
    void handleSessionLoadRequest(const QString &sessionName);
    void onBuildStateChanged();

private:
    bool hasValidProject() const;
    bool triggerAction(const QString &actionId);
    bool triggerDebuggerAction(const QString &actionId);

    bool m_sessionLoadResult;
    
    // Method timeout storage
    QMap<QString, int> m_methodTimeouts;
    
    // Issues management
    IssuesManager *m_issuesManager;
    
    // Build state tracking
    bool m_buildWasInProgress;
    QTimer *m_buildMonitorTimer;
};

} // namespace Internal
} // namespace Qt_MCP_Plugin

#endif // MCPCOMMANDS_H
