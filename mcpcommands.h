#ifndef MCPCOMMANDS_H
#define MCPCOMMANDS_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QTimer>

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
    // filter: "all" (default), "errors", or "warnings"
    QStringList listIssues(const QString &filter = "all");
    
    // Configure error limit for issue tracking
    void setErrorLimit(int limit);
    int errorLimit() const;
    void setStopBuildOnLimit(bool stop);
    bool stopBuildOnLimit() const;
    
    // Build output commands
    QString getCompileOutput();
    QString getApplicationOutput();
    
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
    
    // Debugger state inspection
    QString getCallStack();
    

signals:
    void sessionLoadRequested(const QString &sessionName);
    void buildStateChanged();

private slots:
    void handleSessionLoadRequest(const QString &sessionName);
    void onBuildStateChanged();

private:
    bool hasValidProject() const;
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
