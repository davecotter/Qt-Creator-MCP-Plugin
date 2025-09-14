#ifndef MCPCOMMANDS_H
#define MCPCOMMANDS_H

#include <QObject>
#include <QStringList>

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
    bool openFile(const QString &path);
    QStringList listProjects();
    QStringList listBuildConfigs();
    bool switchToBuildConfig(const QString &name);
    bool quit();
    QString getVersion();

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

signals:
    void sessionLoadRequested(const QString &sessionName);

private slots:
    void handleSessionLoadRequest(const QString &sessionName);

private:
    bool hasValidProject() const;
    bool m_sessionLoadResult;
};

} // namespace Internal
} // namespace Qt_MCP_Plugin

#endif // MCPCOMMANDS_H
