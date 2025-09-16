#pragma once

#include <QObject>
#include <QStringList>
#include <QString>

namespace Qt_MCP_Plugin {
namespace Internal {

/**
 * @brief Manages access to Qt Creator's Issues panel
 * 
 * This class provides a clean interface for accessing and retrieving
 * issues from Qt Creator's Issues panel. It encapsulates the complexity
 * of accessing internal Qt Creator APIs and provides a simple interface
 * for the MCP plugin.
 */
class IssuesManager : public QObject
{
    Q_OBJECT

public:
    explicit IssuesManager(QObject *parent = nullptr);
    ~IssuesManager() override = default;

    /**
     * @brief Retrieves all current issues from the Issues panel
     * @return List of formatted issue strings
     */
    QStringList getCurrentIssues() const;

    /**
     * @brief Checks if the Issues panel is accessible
     * @return true if accessible, false otherwise
     */
    bool isAccessible() const;

    /**
     * @brief Gets the count of current issues
     * @return Number of issues, or -1 if not accessible
     */
    int getIssueCount() const;

private:
    /**
     * @brief Attempts to access the Issues panel through various methods
     * @return true if successful, false otherwise
     */
    bool initializeAccess();

    /**
     * @brief Formats a task into a readable string
     * @param taskType The type of task (Error, Warning, etc.)
     * @param description The task description
     * @param filePath The file path (if available)
     * @param lineNumber The line number (if available)
     * @return Formatted string
     */
    QString formatTask(const QString &taskType, const QString &description, 
                      const QString &filePath = QString(), int lineNumber = -1) const;

    bool m_accessible = false;
};

} // namespace Internal
} // namespace Qt_MCP_Plugin
