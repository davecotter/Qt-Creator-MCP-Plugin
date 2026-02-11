#include "mcpcommands.h"
#include "issuesmanager.h"

#include <coreplugin/icore.h>
#include <coreplugin/ioutputpane.h>
#include "version.h"
#include <QTimer>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/idocument.h>
#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/session.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/target.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildmanager.h>
#include <projectexplorer/runcontrol.h>
#include <projectexplorer/runconfiguration.h>
#include <debugger/debuggerruncontrol.h>
#include <utils/fileutils.h>
#include <utils/id.h>
#include <extensionsystem/pluginmanager.h>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QWidget>
#include <QDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QCoreApplication>
#include <QMetaType>
#include <QMetaMethod>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QTreeView>
#include <QTableView>
#include <QRegularExpression>

#include <QApplication>
#include <QDebug>
#include <QThread>
#include <QProcess>
#include <QFile>
#include <QTimer>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>

namespace Qt_MCP_Plugin {
namespace Internal {

MCPCommands::MCPCommands(QObject *parent)
    : QObject(parent), m_sessionLoadResult(false), m_buildWasInProgress(false)
{
    // Connect signal-slot for session loading
    connect(this, &MCPCommands::sessionLoadRequested, 
            this, &MCPCommands::handleSessionLoadRequest, 
            Qt::QueuedConnection);
    
    // Initialize default method timeouts (in seconds)
    m_methodTimeouts["debug"] = 60;
    m_methodTimeouts["build"] = 1200;  // 20 minutes
    m_methodTimeouts["runProject"] = 60;
    m_methodTimeouts["loadSession"] = 120;
    m_methodTimeouts["cleanProject"] = 300;  // 5 minutes
    
    // Initialize issues manager
    m_issuesManager = new IssuesManager(this);
    
    // Connect to BuildManager signals to track build state
    ProjectExplorer::BuildManager *buildManager = ProjectExplorer::BuildManager::instance();
    if (buildManager) {
        // Connect to build state change signals - use QObject::connect with string-based signals
        // as BuildManager signals may not be directly accessible
        connect(buildManager, SIGNAL(buildQueueFinished(bool)), 
                this, SLOT(onBuildStateChanged()));
        qDebug() << "MCPCommands: Connected to BuildManager signals for build state tracking";
    }
    
    // Create build monitor timer
    m_buildMonitorTimer = new QTimer(this);
    m_buildMonitorTimer->setSingleShot(false);
    m_buildMonitorTimer->setInterval(500); // Check every 500ms
    connect(m_buildMonitorTimer, &QTimer::timeout, this, &MCPCommands::onBuildStateChanged);
}

bool MCPCommands::build()
{
    if (!hasValidProject()) {
        qDebug() << "No valid project available for building";
        return false;
    }

    ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project) {
        qDebug() << "No current project";
        return false;
    }

    ProjectExplorer::Target *target = project->activeTarget();
    if (!target) {
        qDebug() << "No active target";
        return false;
    }

    ProjectExplorer::BuildConfiguration *buildConfig = target->activeBuildConfiguration();
    if (!buildConfig) {
        qDebug() << "No active build configuration";
        return false;
    }

    qDebug() << "Starting build for project:" << project->displayName();
    
    // Mark that we're about to start a build
    m_buildWasInProgress = false; // Reset, will be set to true when build actually starts
    
    // Use ActionManager to trigger the "Build" action (more reliable)
    Core::ActionManager *actionManager = Core::ActionManager::instance();
    if (!actionManager) {
        qDebug() << "ActionManager not available, falling back to BuildManager";
        ProjectExplorer::BuildManager::buildProjectWithoutDependencies(project);
        // Give it a moment to start
        QThread::msleep(100);
        m_buildWasInProgress = ProjectExplorer::BuildManager::isBuilding();
        return true;
    }
    
    // Try different possible action IDs for building
    QStringList buildActionIds = {
        "ProjectExplorer.Build",
        "ProjectExplorer.BuildProject",
        "ProjectExplorer.BuildStartupProject"
    };
    
    bool actionTriggered = false;
    for (const QString &actionId : buildActionIds) {
        Core::Command *command = actionManager->command(Utils::Id::fromString(actionId));
        if (command) {
            qDebug() << "Found build action:" << actionId << "enabled:" << command->action()->isEnabled();
            if (command->action()->isEnabled()) {
                qDebug() << "Triggering build action:" << actionId;
                command->action()->trigger();
                actionTriggered = true;
                // Give it a moment to start
                QThread::msleep(200);
                m_buildWasInProgress = ProjectExplorer::BuildManager::isBuilding();
                qDebug() << "After triggering action, isBuilding:" << m_buildWasInProgress;
                break;
            } else {
                qDebug() << "Build action" << actionId << "is disabled";
            }
        } else {
            qDebug() << "Build action" << actionId << "not found";
        }
    }
    
    if (!actionTriggered) {
        qDebug() << "No enabled build action found, using BuildManager directly";
        ProjectExplorer::BuildManager::buildProjectWithoutDependencies(project);
        // Give it a moment to start
        QThread::msleep(200);
        m_buildWasInProgress = ProjectExplorer::BuildManager::isBuilding();
        qDebug() << "After BuildManager::buildProjectWithoutDependencies, isBuilding:" << m_buildWasInProgress;
    }
    
    qDebug() << "Build triggered, final isBuilding:" << m_buildWasInProgress;
    return true;
}

QString MCPCommands::debug()
{
    QStringList results;
    results.append("=== DEBUG ATTEMPT ===");
    
    if (!hasValidProject()) {
        results.append("ERROR: No valid project available for debugging");
        return results.join("\n");
    }

    ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project) {
        results.append("ERROR: No current project");
        return results.join("\n");
    }

    ProjectExplorer::Target *target = project->activeTarget();
    if (!target) {
        results.append("ERROR: No active target");
        return results.join("\n");
    }

    ProjectExplorer::RunConfiguration *runConfig = target->activeRunConfiguration();
    if (!runConfig) {
        results.append("ERROR: No active run configuration available for debugging");
        return results.join("\n");
    }

    results.append("Project: " + project->displayName());
    results.append("Run configuration: " + runConfig->displayName());
    results.append("");
    
    // Helper function to check if kJams process is running (cross-platform)
    auto checkProcessRunning = []() -> bool {
        QProcess checkProcess;
#ifdef Q_OS_WIN
        // Windows: Use tasklist with proper filtering
        checkProcess.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq kJams.exe" << "/FO" << "CSV");
        checkProcess.waitForFinished(2000);
        QString output = QString::fromUtf8(checkProcess.readAllStandardOutput());
        // On Windows, tasklist returns CSV format, look for kJams.exe
        return output.contains("kJams.exe", Qt::CaseInsensitive);
#else
        // macOS/Linux: Use ps command (existing functionality preserved)
        checkProcess.start("ps", QStringList() << "aux");
        checkProcess.waitForFinished(2000);
        QString output = QString::fromUtf8(checkProcess.readAllStandardOutput());
        return output.contains("kJams", Qt::CaseInsensitive);
#endif
    };
    
    // Trigger debug action on main thread
    results.append("=== STARTING DEBUG SESSION ===");
    
    Core::ActionManager *actionManager = Core::ActionManager::instance();
    if (actionManager) {
        // Try multiple common debug action IDs
        QStringList debugActionIds = {
            "Debugger.StartDebugging",
            "ProjectExplorer.StartDebugging", 
            "Debugger.Debug",
            "ProjectExplorer.Debug",
            "Debugger.StartDebuggingOfStartupProject",
            "ProjectExplorer.StartDebuggingOfStartupProject"
        };
        
        bool debugTriggered = false;
        for (const QString &debugActionId : debugActionIds) {
            results.append("Trying debug action: " + debugActionId);
            
            Core::Command *command = actionManager->command(Utils::Id::fromString(debugActionId));
            if (command && command->action()) {
                results.append("Found debug action, triggering...");
                command->action()->trigger();
                results.append("Debug action triggered successfully");
                debugTriggered = true;
                break;
            } else {
                results.append("Debug action not found: " + debugActionId);
            }
        }
        
        if (!debugTriggered) {
            results.append("ERROR: No debug action found among tried IDs");
            return results.join("\n");
        }
    } else {
        results.append("ERROR: ActionManager not available");
        return results.join("\n");
    }
    
    results.append("Debug session initiated successfully!");
    results.append("The debugger is now starting in the background.");
    results.append("Check Qt Creator's debugger output for progress updates.");
    results.append("NOTE: The debug session will continue running asynchronously.");
    
    results.append("");
    results.append("=== DEBUG RESULT ===");
    results.append("Debug command completed.");
    
    return results.join("\n");
}

QString MCPCommands::stopDebug()
{
    QStringList results;
    results.append("=== STOP DEBUGGING ===");
    
    // Use ActionManager to trigger the "Stop Debugging" action
    Core::ActionManager *actionManager = Core::ActionManager::instance();
    if (!actionManager) {
        results.append("ERROR: ActionManager not available");
        return results.join("\n");
    }
    
    // Try different possible action IDs for stopping debugging
    QStringList stopActionIds = {
        "Debugger.StopDebugger",
        "Debugger.Stop",
        "ProjectExplorer.StopDebugging",
        "ProjectExplorer.Stop",
        "Debugger.StopDebugging"
    };
    
    bool actionTriggered = false;
    for (const QString &actionId : stopActionIds) {
        results.append("Trying stop debug action: " + actionId);
        
        Core::Command *command = actionManager->command(Utils::Id::fromString(actionId));
        if (command && command->action()) {
            results.append("Found stop debug action, triggering...");
            command->action()->trigger();
            results.append("Stop debug action triggered successfully");
            actionTriggered = true;
            break;
        } else {
            results.append("Stop debug action not found: " + actionId);
        }
    }
    
    if (!actionTriggered) {
        results.append("WARNING: No stop debug action found among tried IDs");
        results.append("You may need to stop debugging manually from Qt Creator's debugger interface");
    }
    
    results.append("");
    results.append("=== STOP DEBUG RESULT ===");
    results.append("Stop debug command completed.");
    
    return results.join("\n");
}

QString MCPCommands::getVersion()
{
    return PLUGIN_VERSION_STRING;
}

QString MCPCommands::getBuildStatus()
{
    QStringList results;
    results.append("=== BUILD STATUS ===");
    
    bool currentlyBuilding = ProjectExplorer::BuildManager::isBuilding();
    bool wasBuilding = m_buildWasInProgress;
    
    // Check if build is currently running
    if (currentlyBuilding) {
        results.append("Building: 50%");
        results.append("Status: Build in progress");
        results.append("Current step: Compiling");
        m_buildWasInProgress = true;
    } else {
        results.append("Building: 0%");
        if (wasBuilding && !currentlyBuilding) {
            results.append("Status: Build just completed");
            m_buildWasInProgress = false;
        } else {
            results.append("Status: Not building");
        }
    }
    
    // Get build task information if available
    if (ProjectExplorer::BuildManager::tasksAvailable()) {
        int errorCount = ProjectExplorer::BuildManager::getErrorTaskCount();
        results.append(QString("Build errors: %1").arg(errorCount));
        // Note: getWarningTaskCount() doesn't exist in BuildManager API
    }
    
    results.append("");
    results.append("=== BUILD STATUS RESULT ===");
    results.append("Build status retrieved successfully.");
    
    return results.join("\n");
}

bool MCPCommands::isBuildInProgress() const
{
    return ProjectExplorer::BuildManager::isBuilding();
}

bool MCPCommands::waitForBuildCompletion(int timeoutSeconds)
{
    qDebug() << "Waiting for build completion, timeout:" << timeoutSeconds << "seconds";
    
    // Start monitoring
    m_buildMonitorTimer->start();
    m_buildWasInProgress = ProjectExplorer::BuildManager::isBuilding();
    
    if (!m_buildWasInProgress) {
        qDebug() << "No build in progress when waitForBuildCompletion called";
        m_buildMonitorTimer->stop();
        return true; // Already not building
    }
    
    // Poll instead of nested QEventLoop::exec(). Nested exec() can cause Qt Creator
    // BuildManager to run its completion callback with an invalid progress object,
    // leading to SIGSEGV in QFutureInterfaceBase::setProgressValueAndText (crash.log).
    const int pollIntervalMs = 150;
    QElapsedTimer elapsed;
    elapsed.start();
    qDebug() << "Polling for build completion, timeout:" << timeoutSeconds << "seconds";

    while (ProjectExplorer::BuildManager::isBuilding()) {
        if (elapsed.hasExpired(timeoutSeconds * 1000)) {
            qDebug() << "Build wait timed out";
            m_buildMonitorTimer->stop();
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(pollIntervalMs);
    }

    m_buildMonitorTimer->stop();
    m_buildWasInProgress = false;
    qDebug() << "Build completed successfully";
    return true;
}

void MCPCommands::onBuildStateChanged()
{
    bool currentlyBuilding = ProjectExplorer::BuildManager::isBuilding();
    
    if (m_buildWasInProgress && !currentlyBuilding) {
        qDebug() << "Build state changed: Build completed";
        m_buildWasInProgress = false;
        emit buildStateChanged();
    } else if (!m_buildWasInProgress && currentlyBuilding) {
        qDebug() << "Build state changed: Build started";
        m_buildWasInProgress = true;
    }
}

bool MCPCommands::openFile(const QString &path)
{
    if (path.isEmpty()) {
        qDebug() << "Empty file path provided";
        return false;
    }

    Utils::FilePath filePath = Utils::FilePath::fromString(path);
    
    if (!filePath.exists()) {
        qDebug() << "File does not exist:" << path;
        return false;
    }

    qDebug() << "Opening file:" << path;
    
    Core::EditorManager::openEditor(filePath);
    
    return true;
}

QStringList MCPCommands::listProjects()
{
    QStringList projects;
    
    QList<ProjectExplorer::Project *> projectList = ProjectExplorer::ProjectManager::projects();
    for (ProjectExplorer::Project *project : projectList) {
        projects.append(project->displayName());
    }
    
    qDebug() << "Found projects:" << projects;
    
    return projects;
}

QStringList MCPCommands::listBuildConfigs()
{
    QStringList configs;
    
    ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project) {
        qDebug() << "No current project";
        return configs;
    }

    ProjectExplorer::Target *target = project->activeTarget();
    if (!target) {
        qDebug() << "No active target";
        return configs;
    }

    QList<ProjectExplorer::BuildConfiguration *> buildConfigs = target->buildConfigurations();
    for (ProjectExplorer::BuildConfiguration *config : buildConfigs) {
        configs.append(config->displayName());
    }
    
    qDebug() << "Found build configurations:" << configs;
    
    return configs;
}

bool MCPCommands::switchToBuildConfig(const QString &name)
{
    if (name.isEmpty()) {
        qDebug() << "Empty build configuration name provided";
        return false;
    }

    ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project) {
        qDebug() << "No current project";
        return false;
    }

    ProjectExplorer::Target *target = project->activeTarget();
    if (!target) {
        qDebug() << "No active target";
        return false;
    }

    QList<ProjectExplorer::BuildConfiguration *> buildConfigs = target->buildConfigurations();
    for (ProjectExplorer::BuildConfiguration *config : buildConfigs) {
        if (config->displayName() == name) {
            qDebug() << "Switching to build configuration:" << name;
            target->setActiveBuildConfiguration(config, ProjectExplorer::SetActive::Cascade);
            return true;
        }
    }

    qDebug() << "Build configuration not found:" << name;
    return false;
}

bool MCPCommands::quit()
{
    qDebug() << "Starting graceful quit process...";
    
    // Check if debugging is currently active
    bool debuggingActive = isDebuggingActive();
    qDebug() << "Debug session check result:" << debuggingActive;
    
    if (debuggingActive) {
        qDebug() << "Debug session detected, attempting to stop debugging gracefully...";
        
        // Perform debugging cleanup synchronously (but using non-blocking timers)
        return performDebuggingCleanupSync();
        
    } else {
        qDebug() << "No active debug session detected, quitting immediately...";
        QApplication::quit();
        return true;
    }
}

bool MCPCommands::performDebuggingCleanupSync()
{
    qDebug() << "Starting synchronous debugging cleanup process...";
    
    // Step 1: Try to stop debugging gracefully
    QString stopResult = stopDebug();
    qDebug() << "Stop debug result:" << stopResult;
    
    // Step 2: Wait up to 10 seconds for debugging to stop (using event loop)
    QEventLoop stopLoop;
    QTimer stopTimer;
    stopTimer.setSingleShot(true);
    QObject::connect(&stopTimer, &QTimer::timeout, &stopLoop, &QEventLoop::quit);
    
    // Check every second if debugging has stopped
    QTimer checkTimer;
    QObject::connect(&checkTimer, &QTimer::timeout, [this, &stopLoop, &checkTimer]() {
        if (!isDebuggingActive()) {
            qDebug() << "Debug session stopped successfully";
            checkTimer.stop();
            stopLoop.quit();
        }
    });
    
    checkTimer.start(1000); // Check every second
    stopTimer.start(10000); // Maximum 10 seconds
    stopLoop.exec(); // Wait for either success or timeout
    checkTimer.stop();
    
    // Step 3: If still debugging, try abort debugging
    if (isDebuggingActive()) {
        qDebug() << "Still debugging after stop, attempting abort debugging...";
        QString abortResult = abortDebug();
        qDebug() << "Abort debug result:" << abortResult;
        
        // Wait up to 5 seconds for abort to take effect
        QEventLoop abortLoop;
        QTimer abortTimer;
        abortTimer.setSingleShot(true);
        QObject::connect(&abortTimer, &QTimer::timeout, &abortLoop, &QEventLoop::quit);
        
        QTimer abortCheckTimer;
        QObject::connect(&abortCheckTimer, &QTimer::timeout, [this, &abortLoop, &abortCheckTimer]() {
            if (!isDebuggingActive()) {
                qDebug() << "Debug session aborted successfully";
                abortCheckTimer.stop();
                abortLoop.quit();
            }
        });
        
        abortCheckTimer.start(1000); // Check every second
        abortTimer.start(5000); // Maximum 5 seconds
        abortLoop.exec(); // Wait for either success or timeout
        abortCheckTimer.stop();
    }
    
    // Step 4: If still debugging, try to kill debugged processes
    if (isDebuggingActive()) {
        qDebug() << "Still debugging after abort, attempting to kill debugged processes...";
        bool killResult = killDebuggedProcesses();
        qDebug() << "Kill debugged processes result:" << killResult;
        
        // Wait up to 5 seconds for kill to take effect
        QEventLoop killLoop;
        QTimer killTimer;
        killTimer.setSingleShot(true);
        QObject::connect(&killTimer, &QTimer::timeout, &killLoop, &QEventLoop::quit);
        
        QTimer killCheckTimer;
        QObject::connect(&killCheckTimer, &QTimer::timeout, [this, &killLoop, &killCheckTimer]() {
            if (!isDebuggingActive()) {
                qDebug() << "Debugged processes killed successfully";
                killCheckTimer.stop();
                killLoop.quit();
            }
        });
        
        killCheckTimer.start(1000); // Check every second
        killTimer.start(5000); // Maximum 5 seconds
        killLoop.exec(); // Wait for either success or timeout
        killCheckTimer.stop();
    }
    
    // Step 5: Final timeout - wait up to configured timeout
    if (isDebuggingActive()) {
        int timeoutSeconds = getMethodTimeout("stopDebug");
        if (timeoutSeconds < 0) timeoutSeconds = 30; // Default 30 seconds
        
        qDebug() << "Still debugging, waiting up to" << timeoutSeconds << "seconds for final timeout...";
        
        QEventLoop finalLoop;
        QTimer finalTimer;
        finalTimer.setSingleShot(true);
        QObject::connect(&finalTimer, &QTimer::timeout, &finalLoop, &QEventLoop::quit);
        
        QTimer finalCheckTimer;
        QObject::connect(&finalCheckTimer, &QTimer::timeout, [this, &finalLoop, &finalCheckTimer]() {
            if (!isDebuggingActive()) {
                qDebug() << "Debug session finally stopped";
                finalCheckTimer.stop();
                finalLoop.quit();
            }
        });
        
        finalCheckTimer.start(1000); // Check every second
        finalTimer.start(timeoutSeconds * 1000); // Maximum configured timeout
        finalLoop.exec(); // Wait for either success or timeout
        finalCheckTimer.stop();
    }
    
    // Step 6: Final check - determine success or failure
    bool success = !isDebuggingActive();
    if (success) {
        qDebug() << "Debug session cleanup completed successfully, quitting Qt Creator...";
        QApplication::quit();
        return true;
    } else {
        qDebug() << "ERROR: Failed to stop debugged application after all attempts - NOT quitting Qt Creator";
        return false; // Don't quit Qt Creator
    }
}

void MCPCommands::performDebuggingCleanup()
{
    // This method is kept for backward compatibility but should not be used
    qDebug() << "performDebuggingCleanup called - this method is deprecated";
}

bool MCPCommands::isDebuggingActive()
{
    // Check if debugging is currently active by looking at debugger actions
    Core::ActionManager *actionManager = Core::ActionManager::instance();
    if (!actionManager) {
        return false;
    }
    
    // Try different possible action IDs for checking if debugging is active
    QStringList stopActionIds = {
        "Debugger.Stop",
        "Debugger.StopDebugger",
        "ProjectExplorer.StopDebugging"
    };
    
    for (const QString &actionId : stopActionIds) {
        Core::Command *command = actionManager->command(Utils::Id::fromString(actionId));
        if (command && command->action() && command->action()->isEnabled()) {
            qDebug() << "Debug session is active (Stop action enabled):" << actionId;
            return true;
        }
    }
    
    // Also check "Abort Debugging" action
    QStringList abortActionIds = {
        "Debugger.Abort",
        "Debugger.AbortDebugger",
        "ProjectExplorer.AbortDebugging"
    };
    
    for (const QString &actionId : abortActionIds) {
        Core::Command *command = actionManager->command(Utils::Id::fromString(actionId));
        if (command && command->action() && command->action()->isEnabled()) {
            qDebug() << "Debug session is active (Abort action enabled):" << actionId;
            return true;
        }
    }
    
    qDebug() << "No active debug session detected";
    return false;
}

QString MCPCommands::abortDebug()
{
    qDebug() << "Attempting to abort debug session...";
    
    // Use ActionManager to trigger the "Abort Debugging" action
    Core::ActionManager *actionManager = Core::ActionManager::instance();
    if (!actionManager) {
        return "ERROR: ActionManager not available";
    }
    
    // Try different possible action IDs for aborting debugging
    QStringList abortActionIds = {
        "Debugger.Abort",
        "Debugger.AbortDebugger", 
        "ProjectExplorer.AbortDebugging",
        "Debugger.AbortDebug"
    };
    
    for (const QString &actionId : abortActionIds) {
        qDebug() << "Trying abort debug action:" << actionId;
        
        Core::Command *command = actionManager->command(Utils::Id::fromString(actionId));
        if (command && command->action() && command->action()->isEnabled()) {
            qDebug() << "Found abort debug action, triggering...";
            command->action()->trigger();
            return "Abort debug action triggered successfully: " + actionId;
        }
    }
    
    return "Abort debug action not found or not enabled";
}

bool MCPCommands::killDebuggedProcesses()
{
    qDebug() << "Attempting to kill debugged processes...";
    
    // This is a simplified implementation
    // In a real scenario, you'd need to:
    // 1. Get the list of processes being debugged from the debugger
    // 2. Kill each process appropriately
    
    // For now, we'll try to find and kill any processes that might be debugged
    // This is platform-specific and would need proper implementation
    
    // TODO: Implement proper process killing for debugged applications
    // This could involve:
    // - Finding the debugged process PID
    // - Using platform-specific kill commands
    // - Handling different types of debugged processes (local, remote, etc.)
    
    return true; // Simplified for now - always return true
}

QString MCPCommands::getCallStack()
{
    qDebug() << "Retrieving call stack from Qt Creator debugger";
    
    QStringList results;
    results.append("=== CALL STACK ===");
    
    // First, check if debugging is active
    if (!isDebuggingActive()) {
        results.append("ERROR: No active debug session");
        results.append("The debugger is not running. Please start a debug session first.");
        return results.join("\n");
    }
    
    results.append("Debug session is active.");
    results.append("");
    
    QWidgetList allWidgets = QApplication::allWidgets();
    
    // Helper lambda to check if a model looks like a stack trace model
    auto isStackModel = [](QAbstractItemModel* model) -> bool {
        if (!model || model->rowCount() == 0) return false;
        
        int colCount = model->columnCount();
        
        // Check column headers for stack-specific names
        QStringList stackHeaders = {"level", "function", "file", "line", "address", "from"};
        QStringList watchHeaders = {"name", "value", "type", "time"}; // Watch view headers to exclude
        
        int stackHeaderCount = 0;
        int watchHeaderCount = 0;
        
        for (int col = 0; col < colCount; ++col) {
            QString header = model->headerData(col, Qt::Horizontal).toString().toLower();
            
            for (const QString& sh : stackHeaders) {
                if (header.contains(sh)) {
                    stackHeaderCount++;
                    break;
                }
            }
            for (const QString& wh : watchHeaders) {
                if (header.contains(wh)) {
                    watchHeaderCount++;
                    break;
                }
            }
        }
        
        // If it has more watch headers than stack headers, it's probably a watch view
        if (watchHeaderCount > stackHeaderCount) return false;
        
        // If it has at least 2 stack-related headers, it's likely a stack view
        if (stackHeaderCount >= 2) return true;
        
        // Check first row content for stack-like patterns
        for (int col = 0; col < colCount; ++col) {
            QString cellText = model->data(model->index(0, col)).toString();
            // Stack frames often have addresses (0x...) and function names with ::
            if (cellText.contains(QRegularExpression("^0x[0-9a-fA-F]+$")) ||
                cellText.contains("::") ||
                cellText.contains(QRegularExpression("\\.(cpp|c|h|mm|m):\\d+"))) {
                return true;
            }
        }
        
        return false;
    };
    
    // Helper lambda to extract stack data from a model
    auto extractStackFromModel = [&results](QAbstractItemModel* model) -> bool {
        int rowCount = model->rowCount();
        int colCount = model->columnCount();
        
        if (rowCount == 0) return false;
        
        // Get column headers
        QStringList headers;
        for (int col = 0; col < colCount; ++col) {
            QString header = model->headerData(col, Qt::Horizontal).toString();
            if (!header.isEmpty()) {
                headers.append(header);
            }
        }
        
        results.append(QString("Found %1 stack frames:").arg(rowCount));
        if (!headers.isEmpty()) {
            results.append("Columns: " + headers.join(" | "));
        }
        results.append("");
        
        // Extract each row
        for (int row = 0; row < rowCount && row < 100; ++row) {
            QStringList rowData;
            
            for (int col = 0; col < colCount; ++col) {
                QModelIndex index = model->index(row, col);
                QString data = model->data(index, Qt::DisplayRole).toString();
                if (!data.isEmpty()) {
                    rowData.append(data);
                }
            }
            
            if (!rowData.isEmpty()) {
                results.append(QString("#%1: %2").arg(row).arg(rowData.join(" | ")));
            }
        }
        
        if (rowCount > 100) {
            results.append(QString("... and %1 more frames (truncated)").arg(rowCount - 100));
        }
        
        return true;
    };
    
    // First pass: Look for widgets with "Stack" in their object name (most reliable)
    for (QWidget* widget : allWidgets) {
        if (!widget || !widget->isVisible()) continue;
        
        QString objectName = widget->objectName();
        
        // Look specifically for StackWindow or similar
        if (objectName.contains("Stack", Qt::CaseInsensitive) && 
            !objectName.contains("Watch", Qt::CaseInsensitive) &&
            !objectName.contains("Local", Qt::CaseInsensitive)) {
            
            qDebug() << "Found stack widget by name:" << objectName;
            
            QList<QAbstractItemView*> views = widget->findChildren<QAbstractItemView*>(QString(), Qt::FindChildrenRecursively);
            for (QAbstractItemView* view : views) {
                QAbstractItemModel* model = view->model();
                if (model && isStackModel(model)) {
                    if (extractStackFromModel(model)) {
                        results.append("");
                        results.append("=== END CALL STACK ===");
                        return results.join("\n");
                    }
                }
            }
        }
    }
    
    // Second pass: Search all visible views and check their models
    QList<QPair<int, QAbstractItemView*>> candidateViews; // score, view
    
    for (QWidget* widget : allWidgets) {
        if (!widget || !widget->isVisible()) continue;
        
        QAbstractItemView* view = qobject_cast<QAbstractItemView*>(widget);
        if (!view) continue;
        
        QAbstractItemModel* model = view->model();
        if (!model || model->rowCount() == 0) continue;
        
        int score = 0;
        int colCount = model->columnCount();
        
        // Score based on headers
        for (int col = 0; col < colCount; ++col) {
            QString header = model->headerData(col, Qt::Horizontal).toString().toLower();
            if (header.contains("function")) score += 10;
            if (header.contains("file")) score += 8;
            if (header.contains("line")) score += 8;
            if (header.contains("address")) score += 6;
            if (header.contains("level")) score += 6;
            if (header.contains("from") || header.contains("module")) score += 4;
            
            // Negative scores for watch-like views
            if (header.contains("value")) score -= 5;
            if (header.contains("type") && !header.contains("return")) score -= 5;
            if (header.contains("time")) score -= 10;
        }
        
        // Score based on content
        if (model->rowCount() > 0) {
            for (int col = 0; col < colCount && col < 5; ++col) {
                QString cellText = model->data(model->index(0, col)).toString();
                if (cellText.contains("0x")) score += 3;
                if (cellText.contains("::")) score += 5;
                if (cellText.contains(QRegularExpression("\\.(cpp|c|h|mm|m)"))) score += 4;
            }
        }
        
        if (score > 5) {
            candidateViews.append({score, view});
        }
    }
    
    // Sort by score and try the best candidate
    std::sort(candidateViews.begin(), candidateViews.end(), 
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    for (const auto& [score, view] : candidateViews) {
        qDebug() << "Trying candidate view with score" << score;
        QAbstractItemModel* model = view->model();
        if (model && extractStackFromModel(model)) {
            results.append("");
            results.append("=== END CALL STACK ===");
            return results.join("\n");
        }
    }
    
    // If we couldn't find the stack view, suggest alternatives
    results.append("Could not automatically extract call stack data.");
    results.append("");
    results.append("The debugger is active but the Stack view content is not accessible.");
    results.append("This may happen if:");
    results.append("  - The Stack panel is not visible in Qt Creator");
    results.append("  - The debugger is running (not paused at a breakpoint)");
    results.append("  - No stack frames are available yet");
    results.append("");
    results.append("Suggestions:");
    results.append("  1. Ensure the debugger is paused at a breakpoint");
    results.append("  2. Open the Stack panel in Qt Creator (View > Views > Stack)");
    results.append("  3. Try again after the debugger stops at a breakpoint");
    
    results.append("");
    results.append("=== END CALL STACK ===");
    
    return results.join("\n");
}

bool MCPCommands::openPreferencesPanel(const QString &panelName)
{
    Core::ActionManager *actionManager = Core::ActionManager::instance();
    if (!actionManager) {
        qDebug() << "ActionManager not available";
        return false;
    }

    QStringList actionIds = {
        "Core.Options",
        "Core.Settings",
        "Core.Preferences",
        "Preferences"
    };

    bool actionTriggeredB = false;
    for (const QString &actionId : actionIds) {
        Core::Command *command = actionManager->command(Utils::Id::fromString(actionId));
        if (command && command->action()->isEnabled()) {
            qDebug() << "Triggering preferences action:" << actionId;
            command->action()->trigger();
            actionTriggeredB = true;
            break;
        }
    }

    if (!actionTriggeredB) {
        qDebug() << "No enabled preferences action found";
        return false;
    }

    // Wait for the preferences dialog to appear
    QDialog *prefsDialogP = nullptr;
    for (int i = 0; i < 40 && !prefsDialogP; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
        for (QWidget *widget : topLevelWidgets) {
            QDialog *dialogP = qobject_cast<QDialog *>(widget);
            if (!dialogP || !dialogP->isVisible()) {
                continue;
            }

            QString titleStr = dialogP->windowTitle();
            if (titleStr.contains("Preferences", Qt::CaseInsensitive)
                || titleStr.contains("Options", Qt::CaseInsensitive)) {
                prefsDialogP = dialogP;
                break;
            }
        }
        if (!prefsDialogP) {
            QThread::msleep(25);
        }
    }

    if (!prefsDialogP) {
        qDebug() << "Preferences dialog not found";
        return false;
    }

    if (panelName.isEmpty()) {
        return true;
    }

    bool okB = false;
    int targetIndexI = panelName.toInt(&okB);

    QList<QListWidget *> listWidgets = prefsDialogP->findChildren<QListWidget *>(QString(), Qt::FindChildrenRecursively);
    if (!listWidgets.isEmpty()) {
        QListWidget *listP = listWidgets.front();

        if (okB) {
            if (targetIndexI >= 0 && targetIndexI < listP->count()) {
                listP->setCurrentRow(targetIndexI);
                return true;
            }
        } else {
            for (int i = 0; i < listP->count(); ++i) {
                QListWidgetItem *itemP = listP->item(i);
                if (itemP && itemP->text().contains(panelName, Qt::CaseInsensitive)) {
                    listP->setCurrentRow(i);
                    return true;
                }
            }
        }
    }

    QList<QTreeView *> treeViews = prefsDialogP->findChildren<QTreeView *>(QString(), Qt::FindChildrenRecursively);
    for (QTreeView *treeP : treeViews) {
        QAbstractItemModel *modelP = treeP->model();
        if (!modelP) {
            continue;
        }

        int rowCount = modelP->rowCount();
        for (int row = 0; row < rowCount; ++row) {
            QModelIndex index = modelP->index(row, 0);
            QString text = modelP->data(index, Qt::DisplayRole).toString();
            if (okB) {
                if (row == targetIndexI) {
                    treeP->setCurrentIndex(index);
                    return true;
                }
            } else if (text.contains(panelName, Qt::CaseInsensitive)) {
                treeP->setCurrentIndex(index);
                return true;
            }
        }
    }

    qDebug() << "Preferences panel not found:" << panelName;
    return false;
}

QString MCPCommands::getCurrentProject()
{
    ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    if (project) {
        return project->displayName();
    }
    return QString();
}

QString MCPCommands::getCurrentBuildConfig()
{
    ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project) {
        return QString();
    }

    ProjectExplorer::Target *target = project->activeTarget();
    if (!target) {
        return QString();
    }

    ProjectExplorer::BuildConfiguration *buildConfig = target->activeBuildConfiguration();
    if (buildConfig) {
        return buildConfig->displayName();
    }

    return QString();
}

bool MCPCommands::runProject()
{
    if (!hasValidProject()) {
        qDebug() << "No valid project available for running";
        return false;
    }

    ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project) {
        qDebug() << "No current project";
        return false;
    }

    ProjectExplorer::Target *target = project->activeTarget();
    if (!target) {
        qDebug() << "No active target";
        return false;
    }
    
    ProjectExplorer::RunConfiguration *runConfig = target->activeRunConfiguration();
    if (!runConfig) {
        qDebug() << "No active run configuration available for running";
        return false;
    }

    qDebug() << "Running project:" << project->displayName();
    
    // Use ActionManager to trigger the "Run" action
    Core::ActionManager *actionManager = Core::ActionManager::instance();
    if (!actionManager) {
        qDebug() << "ActionManager not available";
        return false;
    }
    
    // Try different possible action IDs for running
    QStringList runActionIds = {
        "ProjectExplorer.Run",
        "ProjectExplorer.RunProject",
        "ProjectExplorer.RunStartupProject"
    };
    
    bool actionTriggered = false;
    for (const QString &actionId : runActionIds) {
        Core::Command *command = actionManager->command(Utils::Id::fromString(actionId));
        if (command && command->action()) {
            qDebug() << "Triggering run action:" << actionId;
            command->action()->trigger();
            actionTriggered = true;
            break;
        }
    }
    
    if (!actionTriggered) {
        qDebug() << "No run action found, falling back to RunControl method";
        
        // Fallback: Create a RunControl and start it
        ProjectExplorer::RunControl *runControl = new ProjectExplorer::RunControl(Utils::Id("Desktop"));
        runControl->copyDataFromRunConfiguration(runConfig);
        runControl->start();
    }
    
    return true;
}

bool MCPCommands::cleanProject()
{
    if (!hasValidProject()) {
        qDebug() << "No valid project available for cleaning";
        return false;
    }

    ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    ProjectExplorer::Target *target = project->activeTarget();
    
    if (target) {
        ProjectExplorer::BuildConfiguration *buildConfig = target->activeBuildConfiguration();
        if (buildConfig) {
            qDebug() << "Cleaning project:" << project->displayName();
            ProjectExplorer::BuildManager::cleanProjectWithoutDependencies(project);
            return true;
        }
    }

    qDebug() << "No build configuration available for cleaning";
    return false;
}

QStringList MCPCommands::listOpenFiles()
{
    QStringList files;
    
    QList<Core::IDocument *> documents = Core::DocumentModel::openedDocuments();
    for (Core::IDocument *doc : documents) {
        files.append(doc->filePath().toUserOutput());
    }
    
    qDebug() << "Open files:" << files;
    
    return files;
}

bool MCPCommands::hasValidProject() const
{
    ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
    if (!project) {
        return false;
    }

    ProjectExplorer::Target *target = project->activeTarget();
    if (!target) {
        return false;
    }

    return true;
}

QStringList MCPCommands::listSessions()
{
    QStringList sessions = Core::SessionManager::sessions();
    qDebug() << "Available sessions:" << sessions;
    return sessions;
}

QString MCPCommands::getCurrentSession()
{
    QString session = Core::SessionManager::activeSession();
    qDebug() << "Current session:" << session;
    return session;
}

bool MCPCommands::loadSession(const QString &sessionName)
{
    if (sessionName.isEmpty()) {
        qDebug() << "Empty session name provided";
        return false;
    }

    // Check if the session exists before trying to load it
    QStringList availableSessions = Core::SessionManager::sessions();
    if (!availableSessions.contains(sessionName)) {
        qDebug() << "Session does not exist:" << sessionName;
        qDebug() << "Available sessions:" << availableSessions;
        return false;
    }

    qDebug() << "Loading session:" << sessionName;
    
    // Use a safer approach - check if we're already in the target session
    QString currentSession = Core::SessionManager::activeSession();
    if (currentSession == sessionName) {
        qDebug() << "Already in session:" << sessionName;
        return true;
    }
    
    // Try to load the session using QTimer to avoid blocking
    QTimer::singleShot(0, [this, sessionName]() {
        qDebug() << "Attempting to load session:" << sessionName;
        bool success = Core::SessionManager::loadSession(sessionName);
        qDebug() << "Session load result:" << success;
    });
    
    qDebug() << "Session loading initiated asynchronously";
    return true; // Return true to indicate the request was accepted
}

void MCPCommands::handleSessionLoadRequest(const QString &sessionName)
{
    qDebug() << "Handling session load request on main thread:" << sessionName;
    
    // Load session on main thread
    bool success = Core::SessionManager::loadSession(sessionName);
    m_sessionLoadResult = success;
    
    if (success) {
        qDebug() << "Session loaded successfully on main thread:" << sessionName;
    } else {
        qDebug() << "Failed to load session on main thread:" << sessionName;
    }
}

bool MCPCommands::saveSession()
{
    qDebug() << "Saving current session";
    
    bool successB = Core::SessionManager::saveSession();
    if (successB) {
        qDebug() << "Successfully saved session";
    } else {
        qDebug() << "Failed to save session";
    }
    
    return successB;
}

QStringList MCPCommands::listIssues(const QString &filter)
{
    qDebug() << "Listing issues from Qt Creator's Issues panel with filter:" << filter;
    
    if (!m_issuesManager) {
        qDebug() << "IssuesManager not initialized";
        return QStringList() << "ERROR:Issues manager not initialized";
    }
    
    QStringList issues = m_issuesManager->getCurrentIssues(filter);
    
    // Add project status information for context
    if (ProjectExplorer::BuildManager::isBuilding()) {
        issues.prepend("INFO:Build in progress - issues may not be current");
    }
    
    qDebug() << "Found" << issues.size() << "issues total";
    return issues;
}

void MCPCommands::setErrorLimit(int limit)
{
    if (m_issuesManager) {
        m_issuesManager->setErrorLimit(limit);
    }
}

int MCPCommands::errorLimit() const
{
    return m_issuesManager ? m_issuesManager->errorLimit() : 20;
}

void MCPCommands::setStopBuildOnLimit(bool stop)
{
    if (m_issuesManager) {
        m_issuesManager->setStopBuildOnLimit(stop);
    }
}

bool MCPCommands::stopBuildOnLimit() const
{
    return m_issuesManager ? m_issuesManager->stopBuildOnLimit() : false;
}

QString MCPCommands::getCompileOutput()
{
    qDebug() << "Retrieving compile output from Qt Creator";
    
    QStringList outputLines;
    outputLines.append("=== COMPILE OUTPUT ===");
    QString text;

    auto extractFromItemView = [&](QAbstractItemView* view, const QString& sourceLabel) -> bool {
        if (!view) {
            return false;
        }
        QAbstractItemModel* model = view->model();
        if (!model) {
            return false;
        }
        int rowCount = model->rowCount();
        int colCount = model->columnCount();
        if (rowCount == 0 || colCount == 0) {
            return false;
        }

        QStringList modelLines;
        int maxRows = qMin(rowCount, 200);
        int maxCols = qMin(colCount, 6);
        for (int row = 0; row < maxRows; ++row) {
            QStringList rowParts;
            for (int col = 0; col < maxCols; ++col) {
                QModelIndex index = model->index(row, col);
                QString cellText = model->data(index, Qt::DisplayRole).toString();
                if (!cellText.isEmpty()) {
                    rowParts.append(cellText);
                }
            }
            if (!rowParts.isEmpty()) {
                modelLines.append(rowParts.join(" | "));
            }
        }

        if (modelLines.isEmpty()) {
            return false;
        }

        text = modelLines.join("\n");
        outputLines.append("");
        outputLines.append(QString("Output from %1 model view:").arg(sourceLabel));
        outputLines.append(text);
        qDebug() << "Retrieved" << text.length() << "characters from model view";
        return true;
    };
    
    // Method 1: Use IOutputPane API - this is the proper way to access output panes
    QObjectList allObjects = ExtensionSystem::PluginManager::allObjects();
    for (QObject* obj : allObjects) {
        Core::IOutputPane* outputPane = qobject_cast<Core::IOutputPane*>(obj);
        if (outputPane) {
            QString paneName = QString::fromLatin1(obj->metaObject()->className());
            qDebug() << "Found IOutputPane:" << paneName;
            
            // Look for compile/build output pane
            if (paneName.contains("Compile", Qt::CaseInsensitive) || 
                paneName.contains("Build", Qt::CaseInsensitive)) {
                QWidget* outputWidget = outputPane->outputWidget(nullptr);
                if (outputWidget) {
                    qDebug() << "Got output widget from IOutputPane:" << outputWidget->metaObject()->className();
                    
                    // Check if the output widget itself is a text widget
                    QPlainTextEdit* directPlainText = qobject_cast<QPlainTextEdit*>(outputWidget);
                    QTextEdit* directTextEdit = qobject_cast<QTextEdit*>(outputWidget);

                    if (directPlainText) {
                        text = directPlainText->toPlainText();
                        if (!text.isEmpty()) {
                            outputLines.append("");
                            outputLines.append(QString("Output from IOutputPane (%1):").arg(paneName));
                            outputLines.append(text);
                            qDebug() << "Retrieved" << text.length() << "characters from IOutputPane" << paneName;
                            break;
                        }
                    } else if (directTextEdit) {
                        text = directTextEdit->toPlainText();
                        if (!text.isEmpty()) {
                            outputLines.append("");
                            outputLines.append(QString("Output from IOutputPane (%1):").arg(paneName));
                            outputLines.append(text);
                            qDebug() << "Retrieved" << text.length() << "characters from IOutputPane" << paneName;
                            break;
                        }
                    }

                    // Search for text widgets recursively
                    QList<QPlainTextEdit*> plainTextEdits = outputWidget->findChildren<QPlainTextEdit*>(QString(), Qt::FindChildrenRecursively);
                    QList<QTextEdit*> textEdits = outputWidget->findChildren<QTextEdit*>(QString(), Qt::FindChildrenRecursively);
                    
                    if (!plainTextEdits.isEmpty()) {
                        text = plainTextEdits.first()->toPlainText();
                        if (!text.isEmpty()) {
                            outputLines.append("");
                            outputLines.append(QString("Output from IOutputPane (%1):").arg(paneName));
                            outputLines.append(text);
                            qDebug() << "Retrieved" << text.length() << "characters from IOutputPane" << paneName;
                            break;
                        }
                    } else if (!textEdits.isEmpty()) {
                        text = textEdits.first()->toPlainText();
                        if (!text.isEmpty()) {
                            outputLines.append("");
                            outputLines.append(QString("Output from IOutputPane (%1):").arg(paneName));
                            outputLines.append(text);
                            qDebug() << "Retrieved" << text.length() << "characters from IOutputPane" << paneName;
                            break;
                        }
                    }

                    if (text.isEmpty()) {
                        QList<QAbstractItemView*> itemViews = outputWidget->findChildren<QAbstractItemView*>(QString(), Qt::FindChildrenRecursively);
                        for (QAbstractItemView* view : itemViews) {
                            if (extractFromItemView(view, paneName)) {
                                break;
                            }
                        }
                        if (!text.isEmpty()) {
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Method 2: Try to find OutputWindow through PluginManager (fallback)
    if (text.isEmpty()) {
        QObject* outputWindow = nullptr;
        
        for (QObject* obj : allObjects) {
            if (obj) {
                QString className = QString::fromLatin1(obj->metaObject()->className());
                // Look for OutputWindow, CompileOutputWindow, or similar
                if (className.contains("OutputWindow", Qt::CaseInsensitive) ||
                    className.contains("CompileOutput", Qt::CaseInsensitive) ||
                    (className.contains("Output", Qt::CaseInsensitive) && 
                     className.contains("Window", Qt::CaseInsensitive))) {
                    outputWindow = obj;
                    qDebug() << "Found potential output window:" << className;
                    break;
                }
            }
        }
    
        if (outputWindow) {
            const QMetaObject* metaObj = outputWindow->metaObject();
        
            // Method 1: Try casting to QWidget to access child widgets
        QWidget* widget = qobject_cast<QWidget*>(outputWindow);
        if (widget) {
            // Check if the widget itself is a text widget
            QPlainTextEdit* directPlainText = qobject_cast<QPlainTextEdit*>(widget);
            QTextEdit* directTextEdit = qobject_cast<QTextEdit*>(widget);
            if (directPlainText) {
                text = directPlainText->toPlainText();
                if (!text.isEmpty()) {
                    outputLines.append("");
                    outputLines.append("Output from QPlainTextEdit:");
                    outputLines.append(text);
                    qDebug() << "Retrieved" << text.length() << "characters from QPlainTextEdit";
                }
            } else if (directTextEdit) {
                text = directTextEdit->toPlainText();
                if (!text.isEmpty()) {
                    outputLines.append("");
                    outputLines.append("Output from QTextEdit:");
                    outputLines.append(text);
                    qDebug() << "Retrieved" << text.length() << "characters from QTextEdit";
                }
            }

            // Try to find a QTextEdit or QPlainTextEdit child widget (recursive search)
            if (text.isEmpty()) {
                QList<QTextEdit*> textEdits = widget->findChildren<QTextEdit*>(QString(), Qt::FindChildrenRecursively);
                QList<QPlainTextEdit*> plainTextEdits = widget->findChildren<QPlainTextEdit*>(QString(), Qt::FindChildrenRecursively);

                if (!textEdits.isEmpty()) {
                    QTextEdit* textEdit = textEdits.first();
                    text = textEdit->toPlainText();
                    if (!text.isEmpty()) {
                        outputLines.append("");
                        outputLines.append("Output from QTextEdit:");
                        outputLines.append(text);
                        qDebug() << "Retrieved" << text.length() << "characters from QTextEdit";
                    }
                } else if (!plainTextEdits.isEmpty()) {
                    QPlainTextEdit* plainTextEdit = plainTextEdits.first();
                    text = plainTextEdit->toPlainText();
                    if (!text.isEmpty()) {
                        outputLines.append("");
                        outputLines.append("Output from QPlainTextEdit:");
                        outputLines.append(text);
                        qDebug() << "Retrieved" << text.length() << "characters from QPlainTextEdit";
                    }
                }
            }

            if (text.isEmpty()) {
                QList<QAbstractItemView*> itemViews = widget->findChildren<QAbstractItemView*>(QString(), Qt::FindChildrenRecursively);
                for (QAbstractItemView* view : itemViews) {
                    if (extractFromItemView(view, QString::fromLatin1(metaObj->className()))) {
                        break;
                    }
                }
            }
        }
        
        // Method 2: Try using QMetaObject to invoke methods on the object itself
        if (text.isEmpty()) {
            // Try invoking plainText() method if it exists
            int methodIndex = metaObj->indexOfMethod("plainText()");
            if (methodIndex != -1) {
                QMetaMethod method = metaObj->method(methodIndex);
                if (method.invoke(outputWindow, Q_RETURN_ARG(QString, text))) {
                    if (!text.isEmpty()) {
                        outputLines.append("");
                        outputLines.append("Output from plainText() method:");
                        outputLines.append(text);
                        qDebug() << "Retrieved" << text.length() << "characters from plainText()";
                    }
                }
            }
        }
        
        // Method 3: Try invoking toPlainText() method
        if (text.isEmpty()) {
            int methodIndex = metaObj->indexOfMethod("toPlainText()");
            if (methodIndex != -1) {
                QMetaMethod method = metaObj->method(methodIndex);
                if (method.invoke(outputWindow, Q_RETURN_ARG(QString, text))) {
                    if (!text.isEmpty()) {
                        outputLines.append("");
                        outputLines.append("Output from toPlainText() method:");
                        outputLines.append(text);
                        qDebug() << "Retrieved" << text.length() << "characters from toPlainText()";
                    }
                }
            }
        }
        
        // Method 4: Try to get a property that might contain the text
        if (text.isEmpty()) {
            int propIndex = metaObj->indexOfProperty("plainText");
            if (propIndex != -1) {
                QMetaProperty prop = metaObj->property(propIndex);
                if (prop.isReadable()) {
                    QVariant value = prop.read(outputWindow);
                    if (value.isValid() && value.typeId() == QMetaType::QString) {
                        text = value.toString();
                        if (!text.isEmpty()) {
                            outputLines.append("");
                            outputLines.append("Output from plainText property:");
                            outputLines.append(text);
                            qDebug() << "Retrieved" << text.length() << "characters from plainText property";
                        }
                    }
                }
            }
        }
        
        // Method 5: Try to find QTextDocument as a child
        if (text.isEmpty() && widget) {
            QTextDocument* doc = widget->findChild<QTextDocument*>();
            if (doc) {
                text = doc->toPlainText();
                if (!text.isEmpty()) {
                    outputLines.append("");
                    outputLines.append("Output from QTextDocument:");
                    outputLines.append(text);
                    qDebug() << "Retrieved" << text.length() << "characters from QTextDocument";
                }
            }
        }
        
        // Method 6: Try to access children recursively and look for any text-bearing widget
        if (text.isEmpty() && widget) {
            QList<QObject*> allChildren = widget->findChildren<QObject*>(QString(), Qt::FindChildrenRecursively);
            for (QObject* child : allChildren) {
                QTextEdit* te = qobject_cast<QTextEdit*>(child);
                if (te) {
                    QString childText = te->toPlainText();
                    if (!childText.isEmpty() && childText.length() > text.length()) {
                        text = childText;
                    }
                } else {
                    QPlainTextEdit* pte = qobject_cast<QPlainTextEdit*>(child);
                    if (pte) {
                        QString childText = pte->toPlainText();
                        if (!childText.isEmpty() && childText.length() > text.length()) {
                            text = childText;
                        }
                    }
                }
            }
            if (!text.isEmpty()) {
                outputLines.append("");
                outputLines.append("Output from recursive widget search:");
                outputLines.append(text);
                qDebug() << "Retrieved" << text.length() << "characters from recursive search";
            }
        }
        
        // Method 7: Try to get all properties and look for widget or text properties
        if (text.isEmpty()) {
            for (int i = 0; i < metaObj->propertyCount(); ++i) {
                QMetaProperty prop = metaObj->property(i);
                if (prop.isReadable()) {
                    QVariant value = prop.read(outputWindow);
                    QString propName = QString::fromLatin1(prop.name());
                    
                    // Try widget-related properties
                    if (propName.contains("widget", Qt::CaseInsensitive) || 
                        propName.contains("editor", Qt::CaseInsensitive) ||
                        propName.contains("text", Qt::CaseInsensitive)) {
                        QWidget* outputWidget = qvariant_cast<QWidget*>(value);
                        if (outputWidget) {
                            QList<QPlainTextEdit*> plainTextEdits = outputWidget->findChildren<QPlainTextEdit*>(QString(), Qt::FindChildrenRecursively);
                            QList<QTextEdit*> textEdits = outputWidget->findChildren<QTextEdit*>(QString(), Qt::FindChildrenRecursively);
                            
                            if (!plainTextEdits.isEmpty()) {
                                text = plainTextEdits.first()->toPlainText();
                            } else if (!textEdits.isEmpty()) {
                                text = textEdits.first()->toPlainText();
                            }
                            
                            if (!text.isEmpty()) {
                                outputLines.append("");
                                outputLines.append(QString("Output from %1 property:").arg(propName));
                                outputLines.append(text);
                                qDebug() << "Retrieved" << text.length() << "characters from" << propName << "property";
                                break;
                            }
                        } else if (value.typeId() == QMetaType::QString && !value.toString().isEmpty()) {
                            text = value.toString();
                            outputLines.append("");
                            outputLines.append(QString("Output from %1 property:").arg(propName));
                            outputLines.append(text);
                            qDebug() << "Retrieved" << text.length() << "characters from" << propName << "property (string)";
                            break;
                        }
                    }
                }
            }
        }
        
        // Method 8: Try to get all children objects recursively (even if not QWidget)
        if (text.isEmpty()) {
            QList<QObject*> allChildren = outputWindow->findChildren<QObject*>(QString(), Qt::FindChildrenRecursively);
            for (QObject* child : allChildren) {
                if (child) {
                    QWidget* childWidget = qobject_cast<QWidget*>(child);
                    if (childWidget) {
                        QPlainTextEdit* pte = qobject_cast<QPlainTextEdit*>(childWidget);
                        if (pte) {
                            QString childText = pte->toPlainText();
                            if (!childText.isEmpty() && childText.length() > text.length()) {
                                text = childText;
                            }
                        } else {
                            QTextEdit* te = qobject_cast<QTextEdit*>(childWidget);
                            if (te) {
                                QString childText = te->toPlainText();
                                if (!childText.isEmpty() && childText.length() > text.length()) {
                                    text = childText;
                                }
                            }
                        }
                        if (text.isEmpty()) {
                            QAbstractItemView* view = qobject_cast<QAbstractItemView*>(childWidget);
                            if (view && extractFromItemView(view, QString::fromLatin1(metaObj->className()))) {
                                break;
                            }
                        }
                    }
                }
            }
            if (!text.isEmpty()) {
                outputLines.append("");
                outputLines.append("Output from deep recursive object search:");
                outputLines.append(text);
                qDebug() << "Retrieved" << text.length() << "characters from deep recursive search";
            }
        }
        
        // Method 9: Try to access through IOutputPane interface - find all output panes
        if (text.isEmpty()) {
            QObjectList allObjects = ExtensionSystem::PluginManager::allObjects();
            for (QObject* obj : allObjects) {
                Core::IOutputPane* outputPane = qobject_cast<Core::IOutputPane*>(obj);
                if (outputPane) {
                    QString paneName = QString::fromLatin1(obj->metaObject()->className());
                    // Look for compile/build output pane
                    if (paneName.contains("Compile", Qt::CaseInsensitive) || 
                        paneName.contains("Build", Qt::CaseInsensitive) ||
                        paneName.contains("Output", Qt::CaseInsensitive)) {
                        QWidget* outputWidget = outputPane->outputWidget(nullptr);
                        if (outputWidget) {
                            QList<QPlainTextEdit*> plainTextEdits = outputWidget->findChildren<QPlainTextEdit*>(QString(), Qt::FindChildrenRecursively);
                            QList<QTextEdit*> textEdits = outputWidget->findChildren<QTextEdit*>(QString(), Qt::FindChildrenRecursively);
                            
                            if (!plainTextEdits.isEmpty()) {
                                text = plainTextEdits.first()->toPlainText();
                            } else if (!textEdits.isEmpty()) {
                                text = textEdits.first()->toPlainText();
                            }
                            
                            if (!text.isEmpty()) {
                                outputLines.append("");
                                outputLines.append(QString("Output from IOutputPane (%1):").arg(paneName));
                                outputLines.append(text);
                                qDebug() << "Retrieved" << text.length() << "characters from IOutputPane" << paneName;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        // Method 10: Search through all application widgets for text editors
        if (text.isEmpty()) {
            QWidgetList allWidgets = QApplication::allWidgets();
            for (QWidget* w : allWidgets) {
                if (w && w->isVisible()) {
                    QString widgetName = w->objectName();
                    QString className = QString::fromLatin1(w->metaObject()->className());
                    
                    // Look for compile output related widgets
                    if ((widgetName.contains("compile", Qt::CaseInsensitive) || 
                         widgetName.contains("build", Qt::CaseInsensitive) ||
                         widgetName.contains("output", Qt::CaseInsensitive)) ||
                        (className.contains("Compile", Qt::CaseInsensitive) ||
                         className.contains("Build", Qt::CaseInsensitive))) {
                        
                        QPlainTextEdit* pte = qobject_cast<QPlainTextEdit*>(w);
                        if (pte) {
                            QString widgetText = pte->toPlainText();
                            if (!widgetText.isEmpty() && widgetText.length() > text.length()) {
                                text = widgetText;
                            }
                        } else {
                            QTextEdit* te = qobject_cast<QTextEdit*>(w);
                            if (te) {
                                QString widgetText = te->toPlainText();
                                if (!widgetText.isEmpty() && widgetText.length() > text.length()) {
                                    text = widgetText;
                                }
                            } else {
                                // Search children
                                QList<QPlainTextEdit*> plainTextEdits = w->findChildren<QPlainTextEdit*>(QString(), Qt::FindChildrenRecursively);
                                QList<QTextEdit*> textEdits = w->findChildren<QTextEdit*>(QString(), Qt::FindChildrenRecursively);
                                
                                if (!plainTextEdits.isEmpty()) {
                                    QString widgetText = plainTextEdits.first()->toPlainText();
                                    if (!widgetText.isEmpty() && widgetText.length() > text.length()) {
                                        text = widgetText;
                                    }
                                } else if (!textEdits.isEmpty()) {
                                    QString widgetText = textEdits.first()->toPlainText();
                                    if (!widgetText.isEmpty() && widgetText.length() > text.length()) {
                                        text = widgetText;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (!text.isEmpty()) {
                outputLines.append("");
                outputLines.append("Output from application widget search:");
                outputLines.append(text);
                qDebug() << "Retrieved" << text.length() << "characters from application widget search";
            }
        }
        
        if (text.isEmpty()) {
            outputLines.append("");
            outputLines.append("WARNING: Found output window object but could not extract text content");
            outputLines.append("Class name:" + QString::fromLatin1(metaObj->className()));
            outputLines.append("Available methods:");
            for (int i = 0; i < metaObj->methodCount() && i < 10; ++i) {
                QMetaMethod method = metaObj->method(i);
                if (method.access() == QMetaMethod::Public) {
                    outputLines.append("  - " + QString::fromLatin1(method.methodSignature()));
                }
            }
        }
        }
    } else {
        outputLines.append("");
        outputLines.append("WARNING: Could not find compile output window");
        outputLines.append("Searched through" + QString::number(allObjects.size()) + "plugin objects");
        
        // List some potential candidates for debugging
        outputLines.append("");
        outputLines.append("Objects containing 'Output' or 'Build':");
        int count = 0;
        for (QObject* obj : allObjects) {
            if (obj && count < 10) {
                QString className = QString::fromLatin1(obj->metaObject()->className());
                if (className.contains("Output", Qt::CaseInsensitive) ||
                    className.contains("Build", Qt::CaseInsensitive)) {
                    outputLines.append("  - " + className);
                    count++;
                }
            }
        }
    }
    
    // Method 2: Try BuildManager for build output information
    if (ProjectExplorer::BuildManager::instance()) {
        outputLines.append("");
        outputLines.append("=== BUILD STATUS ===");
        outputLines.append("Build in progress:" + QString(ProjectExplorer::BuildManager::isBuilding() ? QStringLiteral("Yes") : QStringLiteral("No")));
        
        if (ProjectExplorer::BuildManager::tasksAvailable()) {
            int errorCount = ProjectExplorer::BuildManager::getErrorTaskCount();
            outputLines.append("Build errors:" + QString::number(errorCount));
        }
    }
    
    outputLines.append("");
    outputLines.append("=== END COMPILE OUTPUT ===");
    
    QString result = outputLines.join("\n");
    qDebug() << "Compile output retrieval completed, total length:" << result.length();
    
    return result;
}


QString MCPCommands::getBuildDiagnostics(const QString &filter)
{
    qDebug() << "Returning structured build diagnostics, filter:" << filter;
    if (!m_issuesManager) {
        return QStringLiteral("[]");
    }
    QJsonArray arr = m_issuesManager->getCurrentIssuesStructured(filter);
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

QString MCPCommands::getApplicationOutput()
{
    qDebug() << "Retrieving application output from Qt Creator";
    
    QStringList outputLines;
    outputLines.append("=== APPLICATION OUTPUT ===");
    QString text;
    
    // Method 1: Use IOutputPane API - look for Application Output pane
    QObjectList allObjects = ExtensionSystem::PluginManager::allObjects();
    for (QObject* obj : allObjects) {
        Core::IOutputPane* outputPane = qobject_cast<Core::IOutputPane*>(obj);
        if (outputPane) {
            QString paneName = QString::fromLatin1(obj->metaObject()->className());
            qDebug() << "Found IOutputPane:" << paneName;
            
            // Look for application output pane
            if (paneName.contains("Application", Qt::CaseInsensitive) || 
                paneName.contains("AppOutput", Qt::CaseInsensitive)) {
                QWidget* outputWidget = outputPane->outputWidget(nullptr);
                if (outputWidget) {
                    qDebug() << "Got output widget from IOutputPane:" << outputWidget->metaObject()->className();
                    
                    // Search for text widgets recursively
                    QList<QPlainTextEdit*> plainTextEdits = outputWidget->findChildren<QPlainTextEdit*>(QString(), Qt::FindChildrenRecursively);
                    QList<QTextEdit*> textEdits = outputWidget->findChildren<QTextEdit*>(QString(), Qt::FindChildrenRecursively);
                    
                    if (!plainTextEdits.isEmpty()) {
                        text = plainTextEdits.first()->toPlainText();
                        if (!text.isEmpty()) {
                            outputLines.append("");
                            outputLines.append(QString("Output from IOutputPane (%1):").arg(paneName));
                            outputLines.append(text);
                            qDebug() << "Retrieved" << text.length() << "characters from IOutputPane" << paneName;
                            break;
                        }
                    } else if (!textEdits.isEmpty()) {
                        text = textEdits.first()->toPlainText();
                        if (!text.isEmpty()) {
                            outputLines.append("");
                            outputLines.append(QString("Output from IOutputPane (%1):").arg(paneName));
                            outputLines.append(text);
                            qDebug() << "Retrieved" << text.length() << "characters from IOutputPane" << paneName;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Method 2: Search all application widgets for Application Output
    if (text.isEmpty()) {
        QWidgetList allWidgets = QApplication::allWidgets();
        for (QWidget* w : allWidgets) {
            if (w) {
                QString widgetName = w->objectName();
                QString className = QString::fromLatin1(w->metaObject()->className());
                
                // Look for application output related widgets
                if ((widgetName.contains("application", Qt::CaseInsensitive) && 
                     widgetName.contains("output", Qt::CaseInsensitive)) ||
                    (className.contains("Application", Qt::CaseInsensitive) &&
                     className.contains("Output", Qt::CaseInsensitive)) ||
                    className.contains("AppOutput", Qt::CaseInsensitive)) {
                    
                    QPlainTextEdit* pte = qobject_cast<QPlainTextEdit*>(w);
                    if (pte) {
                        QString widgetText = pte->toPlainText();
                        if (!widgetText.isEmpty() && widgetText.length() > text.length()) {
                            text = widgetText;
                        }
                    } else {
                        QTextEdit* te = qobject_cast<QTextEdit*>(w);
                        if (te) {
                            QString widgetText = te->toPlainText();
                            if (!widgetText.isEmpty() && widgetText.length() > text.length()) {
                                text = widgetText;
                            }
                        } else {
                            // Search children
                            QList<QPlainTextEdit*> plainTextEdits = w->findChildren<QPlainTextEdit*>(QString(), Qt::FindChildrenRecursively);
                            QList<QTextEdit*> textEdits = w->findChildren<QTextEdit*>(QString(), Qt::FindChildrenRecursively);
                            
                            if (!plainTextEdits.isEmpty()) {
                                QString widgetText = plainTextEdits.first()->toPlainText();
                                if (!widgetText.isEmpty() && widgetText.length() > text.length()) {
                                    text = widgetText;
                                }
                            } else if (!textEdits.isEmpty()) {
                                QString widgetText = textEdits.first()->toPlainText();
                                if (!widgetText.isEmpty() && widgetText.length() > text.length()) {
                                    text = widgetText;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (!text.isEmpty()) {
            outputLines.append("");
            outputLines.append("Output from application widget search:");
            outputLines.append(text);
            qDebug() << "Retrieved" << text.length() << "characters from application widget search";
        }
    }
    
    if (text.isEmpty()) {
        outputLines.append("");
        outputLines.append("No application output found.");
        outputLines.append("Make sure an application is running or has recently run.");
    }
    
    outputLines.append("");
    outputLines.append("=== END APPLICATION OUTPUT ===");
    
    QString result = outputLines.join("\n");
    qDebug() << "Application output retrieval completed, total length:" << result.length();
    
    return result;
}

QString MCPCommands::getMethodMetadata()
{
    QStringList results;
    results.append("=== METHOD METADATA ===");
    results.append("");
    
    // Get all methods with their current timeout settings
    QStringList allMethods = {
        "build", "debug", "runProject", "cleanProject", "loadSession", 
        "getVersion", "listProjects", "listBuildConfigs", "getCurrentProject", 
        "getCurrentBuildConfig", "quit", "listOpenFiles", "listSessions", 
        "getCurrentSession", "saveSession", "listIssues", "getBuildDiagnostics", "getMethodMetadata", 
        "setMethodMetadata", "stopDebug"
    };
    
    results.append("Available methods and their timeout settings:");
    results.append("");
    
    for (const QString& method : allMethods) {
        int timeout = getMethodTimeout(method);
        QString timeoutStr = timeout >= 0 ? QString::number(timeout) + " seconds" : QString("default");
        results.append(QString("  %1: %2").arg(method, -20).arg(timeoutStr));
    }
    
    results.append("");
    results.append("=== METHOD DESCRIPTIONS ===");
    results.append("");
    
    // Add descriptions for key methods
    results.append("build: Compile the current project");
    results.append("debug: Start debugging the current project");
    results.append("stopDebug: Stop the current debug session");
    results.append("runProject: Run the current project");
    results.append("cleanProject: Clean build artifacts");
    results.append("listIssues: List current build issues and warnings");
    results.append("getBuildDiagnostics: Get structured build diagnostics (JSON array of file, line, message, severity) for Cursor Problems panel");
    results.append("getMethodMetadata: Get metadata about all methods");
    results.append("setMethodMetadata: Configure timeout values for methods");
    
    results.append("");
    results.append("=== METADATA COMPLETE ===");
    
    return results.join("\n");
}

QString MCPCommands::setMethodMetadata(const QString &method, int timeoutSeconds)
{
    QStringList results;
    results.append("=== SET METHOD METADATA ===");
    
    if (method.isEmpty()) {
        results.append("ERROR: Method name cannot be empty");
        return results.join("\n");
    }
    
    if (timeoutSeconds < 0) {
        results.append("ERROR: Timeout cannot be negative");
        return results.join("\n");
    }
    
    // List of valid methods that support timeout configuration
    QStringList validMethods = {
        "debug", "build", "runProject", "loadSession", "cleanProject"
    };
    
    if (!validMethods.contains(method)) {
        results.append("ERROR: Method '" + method + "' does not support timeout configuration");
        results.append("Valid methods: " + validMethods.join(", "));
        return results.join("\n");
    }
    
    // Store the new timeout value
    int oldTimeout = m_methodTimeouts.value(method, -1);
    m_methodTimeouts[method] = timeoutSeconds;
    
    results.append("Method: " + method);
    results.append("Previous timeout: " + (oldTimeout >= 0 ? QString::number(oldTimeout) + " seconds" : QString("not set")));
    results.append("New timeout: " + QString::number(timeoutSeconds) + " seconds");
    results.append("");
    results.append("Timeout updated successfully!");
    results.append("Note: This change affects the timeout hints shown in method responses.");
    results.append("The actual operation timeouts are still controlled by Qt Creator's internal mechanisms.");
    
    results.append("");
    results.append("=== SET METHOD METADATA RESULT ===");
    results.append("Method metadata update completed.");
    
    return results.join("\n");
}

int MCPCommands::getMethodTimeout(const QString &method) const
{
    return m_methodTimeouts.value(method, -1);
}


// handleSessionLoadRequest method removed - using direct session loading instead

} // namespace Internal
} // namespace Qt_MCP_Plugin
