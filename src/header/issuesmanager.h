#pragma once

#include <QObject>
#include <QStringList>
#include <QString>
#include <QList>
#include <QJsonArray>

// Need full type for MOC-generated slot code
#include <projectexplorer/task.h>
#include <utils/id.h>

namespace Qt_MCP_Plugin {
namespace Internal {

/**
 * @brief Manages access to Qt Creator's Issues panel
 * 
 * Uses signal-based tracking to capture tasks as they're added.
 * Automatically clears the task list when a new build starts.
 * Stops accumulating when error limit is reached.
 */
class IssuesManager : public QObject
{
    Q_OBJECT

public:
    explicit IssuesManager(QObject *parent = nullptr);
    ~IssuesManager() override = default;

    /**
     * @brief Retrieves current issues from the tracked task list
     * @param filter "all" (default), "errors", or "warnings"
     * @return List of formatted issue strings (errors first, then warnings)
     */
    QStringList getCurrentIssues(const QString &filter = "all") const;

    /**
     * @brief Retrieves current issues as structured JSON for tooling (e.g. Cursor Problems panel)
     * @param filter "all" (default), "errors", or "warnings"
     * @return JSON array of { file, line, column?, message, severity, code? } per issue
     */
    QJsonArray getCurrentIssuesStructured(const QString &filter = "all") const;

    /**
     * @brief Checks if the Issues panel is accessible
     * @return true if accessible, false otherwise
     */
    bool isAccessible() const;

    /**
     * @brief Gets the count of current issues
     * @return Number of issues
     */
    int getIssueCount() const;
    
    /**
     * @brief Sets the maximum number of errors before stopping accumulation
     * @param limit Maximum errors (0 = unlimited)
     */
    void setErrorLimit(int limit);
    
    /**
     * @brief Gets the current error limit
     * @return Current limit
     */
    int errorLimit() const { return m_errorLimit; }
    
    /**
     * @brief Sets whether to stop the build when error limit is reached
     * @param stop true to stop build on limit
     */
    void setStopBuildOnLimit(bool stop) { m_stopBuildOnLimit = stop; }
    
    /**
     * @brief Gets whether build stops on error limit
     * @return true if build will stop
     */
    bool stopBuildOnLimit() const { return m_stopBuildOnLimit; }

private slots:
    // TaskHub signal handlers
    void onTaskAdded(const ProjectExplorer::Task &task);
    void onTaskRemoved(const ProjectExplorer::Task &task);
    void onTasksCleared(Utils::Id categoryId);
    
    // BuildManager signal handler - clears tasks when build starts
    void onBuildStateChanged();

private:
    bool initializeAccess();
    void connectSignals();
    
    QString formatTask(const QString &taskType, const QString &description, 
                      const QString &filePath = QString(), int lineNumber = -1) const;

    bool m_accessible = false;
    bool m_signalsConnected = false;
    
    // Task tracking - cleared at start of each build
    QList<ProjectExplorer::Task> m_trackedTasks;
    
    // Error limit settings
    int m_errorLimit = 20;           // Default: stop after 20 errors
    bool m_stopBuildOnLimit = false; // Whether to cancel build on limit
    int m_errorCount = 0;            // Current error count
    bool m_limitReached = false;     // Whether we've hit the limit
};

} // namespace Internal
} // namespace Qt_MCP_Plugin
