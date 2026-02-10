#include "issuesmanager.h"

#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>
#include <projectexplorer/buildmanager.h>
#include <projectexplorer/taskhub.h>

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

namespace Qt_MCP_Plugin {
namespace Internal {

IssuesManager::IssuesManager(QObject *parent)
    : QObject(parent)
{
    initializeAccess();
    connectSignals();
}

QStringList IssuesManager::getCurrentIssues(const QString &filter) const
{
    QStringList issues;
    
    // Determine filter mode
    bool showErrors = (filter == QStringLiteral("all") || filter == QStringLiteral("errors"));
    bool showWarnings = (filter == QStringLiteral("all") || filter == QStringLiteral("warnings"));
    
    QString filterDesc = filter == QStringLiteral("errors") ? QStringLiteral("ERRORS ONLY") : 
                        filter == QStringLiteral("warnings") ? QStringLiteral("WARNINGS ONLY") : QStringLiteral("ALL ISSUES");
    issues.append(QString("=== CURRENT BUILD ISSUES (%1) ===").arg(filterDesc));
    
    // Separate errors and warnings for ordered output
    QStringList errorList;
    QStringList warningList;
    QStringList otherList;
    
    int errorCount = 0;
    int warningCount = 0;
    
    // Process tracked tasks - separate by type
    for (const ProjectExplorer::Task& task : m_trackedTasks) {
        bool isError = (task.type() == ProjectExplorer::Task::Error);
        bool isWarning = (task.type() == ProjectExplorer::Task::Warning);
        
        QString taskType = isError ? QStringLiteral("ERROR") : 
                          isWarning ? QStringLiteral("WARNING") : QStringLiteral("INFO");
        QString taskInfo = formatTask(
            taskType,
            task.description(),
            task.file().toUserOutput(),
            task.line()
        );
        
        if (isError) {
            errorCount++;
            if (showErrors) {
                errorList.append(taskInfo);
            }
        } else if (isWarning) {
            warningCount++;
            if (showWarnings) {
                warningList.append(taskInfo);
            }
        } else {
            if (filter == QStringLiteral("all")) {
                otherList.append(taskInfo);
            }
        }
    }
    
    // Add errors first, then warnings, then other
    issues.append(errorList);
    issues.append(warningList);
    issues.append(otherList);
    
    // Add limit reached notice if applicable
    if (m_limitReached) {
        issues.append(QString());
        issues.append(QString("*** ERROR LIMIT REACHED (%1 errors) - stopped accumulating ***").arg(m_errorLimit));
    }
    
    // Summary
    if (m_trackedTasks.isEmpty()) {
        issues.append(QStringLiteral("No issues found"));
    } else {
        issues.append(QString());
        issues.append(QStringLiteral("=== SUMMARY ==="));
        issues.append(QString("Errors: %1, Warnings: %2").arg(errorCount).arg(warningCount));
        int shownCount = errorList.size() + warningList.size() + otherList.size();
        issues.append(QString("Shown: %1 (filter: %2)").arg(shownCount).arg(filter));
    }
    
    return issues;
}

QJsonArray IssuesManager::getCurrentIssuesStructured(const QString &filter) const
{
    QJsonArray out;
    bool showErrors = (filter == QStringLiteral("all") || filter == QStringLiteral("errors"));
    bool showWarnings = (filter == QStringLiteral("all") || filter == QStringLiteral("warnings"));

    for (const ProjectExplorer::Task &task : m_trackedTasks) {
        bool isError = (task.type() == ProjectExplorer::Task::Error);
        bool isWarning = (task.type() == ProjectExplorer::Task::Warning);
        if (isError && !showErrors) continue;
        if (isWarning && !showWarnings) continue;
        if (!isError && !isWarning && filter != QStringLiteral("all")) continue;

        QString severity = isError ? QStringLiteral("error") : (isWarning ? QStringLiteral("warning") : QStringLiteral("info"));
        QString path = task.file().toUserOutput();
        int line = task.line();
        if (line < 1) line = 1;

        QJsonObject obj;
        obj.insert(QStringLiteral("file"), path);
        obj.insert(QStringLiteral("line"), line);
        obj.insert(QStringLiteral("column"), 0);
        obj.insert(QStringLiteral("message"), task.description());
        obj.insert(QStringLiteral("severity"), severity);
        obj.insert(QStringLiteral("code"), QStringLiteral("unknown"));
        out.append(obj);
    }
    return out;
}

bool IssuesManager::isAccessible() const
{
    return m_accessible;
}

int IssuesManager::getIssueCount() const
{
    return m_trackedTasks.size();
}

void IssuesManager::setErrorLimit(int limit)
{
    m_errorLimit = limit;
    qDebug() << "IssuesManager: Error limit set to" << limit;
}

bool IssuesManager::initializeAccess()
{
    if (ProjectExplorer::BuildManager::instance()) {
        m_accessible = true;
        qDebug() << "IssuesManager: Initialized (error limit:" << m_errorLimit << ")";
        return true;
    }
    
    qDebug() << "IssuesManager: BuildManager not accessible";
    return false;
}

void IssuesManager::connectSignals()
{
    if (m_signalsConnected) {
        return;
    }
    
    // Connect to TaskHub signals for task tracking
    ProjectExplorer::TaskHub& hub = ProjectExplorer::taskHub();
    
    connect(&hub, &ProjectExplorer::TaskHub::taskAdded,
            this, &IssuesManager::onTaskAdded);
    connect(&hub, &ProjectExplorer::TaskHub::taskRemoved,
            this, &IssuesManager::onTaskRemoved);
    connect(&hub, &ProjectExplorer::TaskHub::tasksCleared,
            this, &IssuesManager::onTasksCleared);
    
    qDebug() << "IssuesManager: Connected to TaskHub signals";
    
    // Connect to BuildManager to detect build start
    connect(ProjectExplorer::BuildManager::instance(), 
            &ProjectExplorer::BuildManager::buildStateChanged,
            this, &IssuesManager::onBuildStateChanged);
    
    qDebug() << "IssuesManager: Connected to BuildManager::buildStateChanged";
    
    m_signalsConnected = true;
}

void IssuesManager::onTaskAdded(const ProjectExplorer::Task &task)
{
    // Don't accumulate if we've hit the error limit
    if (m_limitReached) {
        return;
    }
    
    bool isError = (task.type() == ProjectExplorer::Task::Error);
    
    // Track the task
    m_trackedTasks.append(task);
    
    // Count errors
    if (isError) {
        m_errorCount++;
        
        qDebug() << "IssuesManager: Error" << m_errorCount << "/" << m_errorLimit 
                 << "-" << task.description().left(60);
        
        // Check if we've hit the limit
        if (m_errorLimit > 0 && m_errorCount >= m_errorLimit) {
            m_limitReached = true;
            qDebug() << "IssuesManager: ERROR LIMIT REACHED (" << m_errorLimit << " errors)";
            
            // Optionally stop the build
            if (m_stopBuildOnLimit && ProjectExplorer::BuildManager::isBuilding()) {
                qDebug() << "IssuesManager: Cancelling build due to error limit";
                ProjectExplorer::BuildManager::cancel();
            }
        }
    } else if (task.type() == ProjectExplorer::Task::Warning) {
        qDebug() << "IssuesManager: Warning -" << task.description().left(60);
    }
}

void IssuesManager::onTaskRemoved(const ProjectExplorer::Task &task)
{
    // Find and remove by matching key fields
    for (int i = 0; i < m_trackedTasks.size(); ++i) {
        const ProjectExplorer::Task& t = m_trackedTasks[i];
        if (t.description() == task.description() &&
            t.file() == task.file() &&
            t.line() == task.line()) {
            
            // Adjust error count if removing an error
            if (t.type() == ProjectExplorer::Task::Error && m_errorCount > 0) {
                m_errorCount--;
            }
            
            m_trackedTasks.removeAt(i);
            break;
        }
    }
}

void IssuesManager::onTasksCleared(Utils::Id categoryId)
{
    Q_UNUSED(categoryId);
    qDebug() << "IssuesManager: Tasks cleared";
    m_trackedTasks.clear();
    m_errorCount = 0;
    m_limitReached = false;
}

void IssuesManager::onBuildStateChanged()
{
    // Clear when a build starts
    if (ProjectExplorer::BuildManager::isBuilding()) {
        qDebug() << "IssuesManager: Build started - clearing previous tasks";
        m_trackedTasks.clear();
        m_errorCount = 0;
        m_limitReached = false;
    }
}

QString IssuesManager::formatTask(const QString &taskType, const QString &description, 
                                 const QString &filePath, int lineNumber) const
{
    QString formatted = QString("%1:%2").arg(taskType, description);
    
    if (!filePath.isEmpty()) {
        formatted += QString(" [%1").arg(filePath);
        if (lineNumber > 0) {
            formatted += QString(":%1").arg(lineNumber);
        }
        formatted += QStringLiteral("]");
    }
    
    return formatted;
}

} // namespace Internal
} // namespace Qt_MCP_Plugin
