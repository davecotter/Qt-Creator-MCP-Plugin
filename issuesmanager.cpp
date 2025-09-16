#include "issuesmanager.h"

#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>
#include <projectexplorer/buildmanager.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/task.h>
#include <projectexplorer/taskhub.h>

#include <QDebug>

namespace Qt_MCP_Plugin {
namespace Internal {

IssuesManager::IssuesManager(QObject *parent)
    : QObject(parent)
{
    initializeAccess();
}

QStringList IssuesManager::getCurrentIssues() const
{
    QStringList issues;
    
    if (!m_accessible) {
        issues.append("ERROR:Issues panel not accessible - cannot retrieve current issues");
        return issues;
    }

    // Try to get issues through BuildManager first
    if (ProjectExplorer::BuildManager::tasksAvailable()) {
        int errorCount = ProjectExplorer::BuildManager::getErrorTaskCount();
        if (errorCount > 0) {
            issues.append(QString("INFO:Found %1 error(s) in build system").arg(errorCount));
        } else {
            issues.append("INFO:No build errors found");
        }
    } else {
        issues.append("INFO:No build tasks available");
    }

    // For now, we'll provide project status information
    // TODO: Implement full Issues panel access when the correct API is identified
    issues.append("INFO:Full Issues panel integration requires access to internal Qt Creator APIs");
    issues.append("INFO:To see actual build issues, check the Issues panel in Qt Creator");
    
    return issues;
}

bool IssuesManager::isAccessible() const
{
    return m_accessible;
}

int IssuesManager::getIssueCount() const
{
    if (!m_accessible) {
        return -1;
    }
    
    if (ProjectExplorer::BuildManager::tasksAvailable()) {
        return ProjectExplorer::BuildManager::getErrorTaskCount();
    }
    
    return 0;
}

bool IssuesManager::initializeAccess()
{
    // Check if we can access the BuildManager
    if (ProjectExplorer::BuildManager::instance()) {
        m_accessible = true;
        qDebug() << "IssuesManager: Successfully initialized with BuildManager access";
        return true;
    }
    
    qDebug() << "IssuesManager: Failed to initialize - BuildManager not accessible";
    return false;
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
        formatted += "]";
    }
    
    return formatted;
}

} // namespace Internal
} // namespace Qt_MCP_Plugin
