#include "mcpcommands.h"

#include <coreplugin/icore.h>
#include "version.h"
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/idocument.h>
#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/session.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <projectexplorer/taskhub.h>
#include <projectexplorer/task.h>
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

#include <QApplication>
#include <QDebug>
#include <QThread>
#include <QProcess>
#include <QFile>

namespace Qt_MCP_Plugin {
namespace Internal {

MCPCommands::MCPCommands(QObject *parent)
    : QObject(parent), m_sessionLoadResult(false)
{
    // Connect signal-slot for session loading
    connect(this, &MCPCommands::sessionLoadRequested, 
            this, &MCPCommands::handleSessionLoadRequest, 
            Qt::QueuedConnection);
    
    // Initialize default method timeouts (in seconds)
    m_methodTimeouts["debug"] = 60;
    m_methodTimeouts["build"] = 1200;  // 20 minutes
    m_methodTimeouts["runProject"] = 60;
    m_methodTimeouts["loadSession"] = 30;
    m_methodTimeouts["cleanProject"] = 300;  // 5 minutes
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
    
    // Trigger build
    ProjectExplorer::BuildManager::buildProjectWithoutDependencies(project);
    
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
    qDebug() << "Quitting Qt Creator";
    
    // Close Qt Creator
    QApplication::quit();
    
    return true;
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
    
    // Reset result flag
    m_sessionLoadResult = false;
    
    // Emit signal to load session on main thread
    emit sessionLoadRequested(sessionName);
    
    qDebug() << "Session loading initiated, waiting for completion...";
    
    // Wait for session to finish loading (up to 15 seconds)
    for (int i = 0; i < 15; i++) {
        QThread::msleep(1000); // Wait 1 second
        
        // Check if session is loaded by checking if we have a current project
        ProjectExplorer::Project *project = ProjectExplorer::ProjectManager::startupProject();
        if (project && !project->displayName().isEmpty()) {
            qDebug() << "Session loaded successfully after" << (i + 1) << "seconds";
            return true;
        }
    }
    
    qDebug() << "Session loading timed out after 15 seconds";
    return false;
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

QStringList MCPCommands::listIssues()
{
    qDebug() << "Listing issues from Qt Creator's Issues panel";
    
    QStringList issues;
    
    // Note: Issues panel integration is complex and requires access to internal Qt Creator APIs
    // For now, we'll provide project status information and indicate the limitation
    
    // For now, let's try a different approach - check if there are any recent build results
    // that might contain issues
    if (ProjectExplorer::BuildManager::isBuilding()) {
        issues.append("INFO:Build in progress - issues may not be current");
        qDebug() << "Build is currently in progress";
    }
    
    // Get current project information
    ProjectExplorer::Project *currentProject = ProjectExplorer::ProjectManager::startupProject();
    if (!currentProject) {
        issues.append("WARNING:No active project found");
        qDebug() << "No active project found";
        return issues;
    }
    
    qDebug() << "Current project:" << currentProject->displayName();
    
    // Check if the project has any targets
    QList<ProjectExplorer::Target*> targets = currentProject->targets();
    if (targets.isEmpty()) {
        issues.append("WARNING:Project has no build targets configured");
        qDebug() << "Project has no build targets";
        return issues;
    }
    
    // Check the active target
    ProjectExplorer::Target *activeTarget = currentProject->activeTarget();
    if (!activeTarget) {
        issues.append("WARNING:No active build target");
        qDebug() << "No active build target";
        return issues;
    }
    
    qDebug() << "Active target:" << activeTarget->displayName();
    
    // Check build configurations
    QList<ProjectExplorer::BuildConfiguration*> buildConfigs = activeTarget->buildConfigurations();
    if (buildConfigs.isEmpty()) {
        issues.append("WARNING:No build configurations found");
        qDebug() << "No build configurations found";
        return issues;
    }
    
    // Check the active build configuration
    ProjectExplorer::BuildConfiguration *activeBuildConfig = activeTarget->activeBuildConfiguration();
    if (!activeBuildConfig) {
        issues.append("WARNING:No active build configuration");
        qDebug() << "No active build configuration";
        return issues;
    }
    
    qDebug() << "Active build config:" << activeBuildConfig->displayName();
    
    // Check if build directory exists and is accessible
    Utils::FilePath buildDir = activeBuildConfig->buildDirectory();
    if (!buildDir.exists()) {
        issues.append("WARNING:Build directory does not exist:" + buildDir.toUserOutput());
        qDebug() << "Build directory does not exist:" << buildDir.toUserOutput();
    } else {
        qDebug() << "Build directory exists:" << buildDir.toUserOutput();
    }
    
    // For now, return a message indicating that we need to implement proper Issues panel access
    if (issues.isEmpty()) {
        issues.append("INFO:Issues panel integration not yet fully implemented");
        issues.append("INFO:This command currently shows project status information");
        issues.append("INFO:To see actual build issues, check the Issues panel in Qt Creator");
        qDebug() << "Issues panel integration not yet fully implemented";
    }
    
    qDebug() << "Found" << issues.size() << "issues total";
    return issues;
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
