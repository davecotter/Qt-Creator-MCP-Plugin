#include "qt_mcp_pluginconstants.h"
#include "qt_mcp_plugintr.h"
#include "mcpserver.h"
#include "mcpcommands.h"
#include "version.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>
#include <coreplugin/messagemanager.h>

#include <extensionsystem/iplugin.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QDebug>
#include <iostream>
#include <QTextStream>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QPalette>
#include <QTextEdit>
#include <QScrollArea>

using namespace Core;

namespace Qt_MCP_Plugin::Internal {

class MCPServerStatusDialog : public QDialog
{
	Q_OBJECT

public:
	explicit MCPServerStatusDialog(MCPServer *server, QWidget *parent = nullptr)
		: QDialog(parent)
		, m_serverP(server)
	{
		setWindowTitle(QString("MCP Plugin Status v%1").arg(PLUGIN_VERSION_STRING));
		setModal(true);
		resize(400, 200);
		
		QVBoxLayout *layout = new QVBoxLayout(this);
		
		// Title
		QLabel *titleLabel = new QLabel(QString("Qt MCP Plugin v%1").arg(PLUGIN_VERSION_STRING));
		QFont titleFont = titleLabel->font();
		titleFont.setPointSize(14);
		titleFont.setBold(true);
		titleLabel->setFont(titleFont);
		layout->addWidget(titleLabel);
		
		// Status section
		QHBoxLayout *statusLayout = new QHBoxLayout();
		
		m_statusIcon = new QLabel();
		m_statusIcon->setFixedSize(24, 24);
		m_statusIcon->setAlignment(Qt::AlignCenter);
		
		m_statusLabel = new QLabel();
		QFont statusFont = m_statusLabel->font();
		statusFont.setPointSize(12);
		m_statusLabel->setFont(statusFont);
		
		statusLayout->addWidget(m_statusIcon);
		statusLayout->addWidget(m_statusLabel);
		statusLayout->addStretch();
		
		layout->addLayout(statusLayout);
		
		// Details
		m_detailsLabel = new QLabel();
		m_detailsLabel->setWordWrap(true);
		m_detailsLabel->setStyleSheet("QLabel { color: #666; }");
		layout->addWidget(m_detailsLabel);
		
		layout->addStretch();
		
		// Buttons
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		buttonLayout->addStretch();
		
		m_restartButton = new QPushButton(Tr::tr("Restart Server"));
		m_restartButton->setEnabled(false);
		connect(m_restartButton, &QPushButton::clicked, this, &MCPServerStatusDialog::restartServer);
		buttonLayout->addWidget(m_restartButton);
		
		QPushButton *closeButton = new QPushButton(Tr::tr("Close"));
		connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
		buttonLayout->addWidget(closeButton);
		
		layout->addLayout(buttonLayout);
		
		updateStatus();
	}

private slots:
	void restartServer()
	{
		if (m_serverP) {
			m_serverP->stop();
			if (m_serverP->start()) {
				updateStatus();
			}
		}
	}

private:
	void updateStatus()
	{
		bool isRunning = m_serverP && m_serverP->isRunning();
		
		if (isRunning) {
			// Green checkmark
			m_statusIcon->setText("✓");
			m_statusIcon->setStyleSheet("QLabel { color: green; font-size: 18px; font-weight: bold; }");
			m_statusLabel->setText(Tr::tr("MCP Server is Running"));
			m_statusLabel->setStyleSheet("QLabel { color: green; }");
			
			// Get the actual port from the server
			quint16 port = m_serverP->getPort();
			QString portInfo = Tr::tr("Server is active on port %1 and accepting connections.").arg(port);
			m_detailsLabel->setText(portInfo);
			
			m_restartButton->setEnabled(false);
		} else {
			// Red X
			m_statusIcon->setText("✗");
			m_statusIcon->setStyleSheet("QLabel { color: red; font-size: 18px; font-weight: bold; }");
			m_statusLabel->setText(Tr::tr("MCP Server is Not Running"));
			m_statusLabel->setStyleSheet("QLabel { color: red; }");
			
			m_detailsLabel->setText(Tr::tr("The MCP server is not active. Click 'Restart Server' to try starting it again."));
			
			m_restartButton->setEnabled(true);
		}
	}

	MCPServer *m_serverP;
	QLabel *m_statusIcon;
	QLabel *m_statusLabel;
	QLabel *m_detailsLabel;
	QPushButton *m_restartButton;
};

class Qt_MCP_PluginPlugin final : public ExtensionSystem::IPlugin
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "Qt_MCP_Plugin.json")

public:
	Qt_MCP_PluginPlugin() = default;

	~Qt_MCP_PluginPlugin() final
	{
		delete m_serverP;
		delete m_commandsP;
	}

	void initialize() final
	{
		// Create the MCP server and commands
		m_serverP = new MCPServer(this);
		m_commandsP = new MCPCommands(this);

		// Initialize the server
		if (!m_serverP->start()) {
			QMessageBox::warning(ICore::dialogParent(),
							   Tr::tr("MCP Plugin"),
							   Tr::tr("Failed to start MCP server"));
		} else {
			// Show startup message in General Messages panel
			outputMessage(QString("MCP Plugin v%1 loaded and functioning - MCP server running on port %2")
				.arg(PLUGIN_VERSION_STRING)
				.arg(m_serverP->getPort()));
		}

		// Create the MCP Plugin menu
		ActionContainer *menu = ActionManager::createMenu(Constants::MENU_ID);
		menu->menu()->setTitle(Tr::tr("MCP Plugin"));
		ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

		// Add separator for About
		menu->addSeparator();

		// About action (shows the existing status dialog)
		ActionBuilder(this, Constants::ABOUT_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(QString("About MCP Plugin v%1").arg(PLUGIN_VERSION_STRING))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::showAbout);

		// Add separator between About and commands
		menu->addSeparator();

		// MCP Command actions
		ActionBuilder(this, Constants::GET_VERSION_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Get Version"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeGetVersion);

		ActionBuilder(this, Constants::LIST_SESSIONS_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("List Sessions"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeListSessions);

		ActionBuilder(this, Constants::LIST_PROJECTS_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("List Projects"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeListProjects);

		ActionBuilder(this, Constants::LIST_BUILD_CONFIGS_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("List Build Configs"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeListBuildConfigs);

		ActionBuilder(this, Constants::GET_CURRENT_PROJECT_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Get Current Project"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeGetCurrentProject);

		ActionBuilder(this, Constants::GET_CURRENT_BUILD_CONFIG_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Get Current Build Config"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeGetCurrentBuildConfig);

		ActionBuilder(this, Constants::GET_CURRENT_SESSION_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Get Current Session"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeGetCurrentSession);

		ActionBuilder(this, Constants::LIST_OPEN_FILES_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("List Open Files"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeListOpenFiles);

		ActionBuilder(this, Constants::LIST_ISSUES_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("List Issues"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeListIssues);

		ActionBuilder(this, Constants::SET_METHOD_METADATA_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Set Method Metadata"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeSetMethodMetadata);

		menu->addSeparator();

		ActionBuilder(this, Constants::BUILD_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Build Project"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeBuild);

		ActionBuilder(this, Constants::RUN_PROJECT_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Run Project"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeRunProject);

		ActionBuilder(this, Constants::DEBUG_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Debug Project"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeDebug);

		ActionBuilder(this, Constants::STOP_DEBUG_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Stop Debugging"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeStopDebug);

		ActionBuilder(this, Constants::CLEAN_PROJECT_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Clean Project"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeCleanProject);

		ActionBuilder(this, Constants::SAVE_SESSION_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Save Session"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeSaveSession);

		menu->addSeparator();

		ActionBuilder(this, Constants::QUIT_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("Quit Qt Creator"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeQuit);
	}

	void extensionsInitialized() final
	{
		// Retrieve objects from the plugin manager's object pool, if needed. (rare)
		// In the extensionsInitialized function, a plugin can be sure that all
		// plugins that depend on it have passed their initialize() and
		// extensionsInitialized() phase.
	}

	ShutdownFlag aboutToShutdown() final
	{
		// Save settings
		// Disconnect from signals that are not needed during shutdown
		// Hide UI (if you add UI that is not in the main window directly)
		
		if (m_serverP) {
			m_serverP->stop();
		}
		
		return SynchronousShutdown;
	}

private:
	void outputMessage(const QString &message)
	{
		// Try different MessageManager methods
		Core::MessageManager::writeFlashing(message);
	}

	void showAbout()
	{
		MCPServerStatusDialog dialog(m_serverP, ICore::dialogParent());
		dialog.exec();
	}

	void executeGetVersion()
	{
		QString version = m_commandsP->getVersion();
		outputMessage(QString("MCP Plugin Version: %1").arg(version));
	}

	void executeListSessions()
	{
		QStringList sessions = m_commandsP->listSessions();
		outputMessage(QString("Available Sessions: %1").arg(sessions.join(", ")));
	}

	void executeListProjects()
	{
		QStringList projects = m_commandsP->listProjects();
		outputMessage(QString("Loaded Projects: %1").arg(projects.join(", ")));
	}

	void executeListBuildConfigs()
	{
		QStringList configs = m_commandsP->listBuildConfigs();
		outputMessage(QString("Build Configurations: %1").arg(configs.join(", ")));
	}

	void executeGetCurrentProject()
	{
		QString project = m_commandsP->getCurrentProject();
		outputMessage(QString("Current Project: %1").arg(project));
	}

	void executeGetCurrentBuildConfig()
	{
		QString config = m_commandsP->getCurrentBuildConfig();
		outputMessage(QString("Current Build Config: %1").arg(config));
	}

	void executeGetCurrentSession()
	{
		QString session = m_commandsP->getCurrentSession();
		outputMessage(QString("Current Session: %1").arg(session));
	}

	void executeListOpenFiles()
	{
		QStringList files = m_commandsP->listOpenFiles();
		outputMessage(QString("Open Files: %1").arg(files.join(", ")));
	}

	void executeListIssues()
	{
		QStringList issues = m_commandsP->listIssues();
		outputMessage(QString("Build Issues: %1").arg(issues.join(", ")));
	}

	void executeSetMethodMetadata()
	{
		outputMessage("Setting method metadata...");
		// For demonstration, set debug timeout to 120 seconds
		QString result = m_commandsP->setMethodMetadata("debug", 120);
		outputMessage(result);
	}

	void executeBuild()
	{
		outputMessage("Starting build...");
		bool success = m_commandsP->build();
		QString result = success ? QStringLiteral("Build started successfully") : QStringLiteral("Build failed to start");
		outputMessage(QString("Build result: %1").arg(result));
	}

	void executeRunProject()
	{
		outputMessage("Running project...");
		bool success = m_commandsP->runProject();
		QString result = success ? QStringLiteral("Project run started successfully") : QStringLiteral("Project run failed to start");
		outputMessage(QString("Run result: %1").arg(result));
	}

	void executeDebug()
	{
		outputMessage("Starting debug session...");
		QString result = m_commandsP->debug();
		outputMessage(QString("Debug result: %1").arg(result));
	}

	void executeStopDebug()
	{
		outputMessage("Stopping debug session...");
		QString result = m_commandsP->stopDebug();
		outputMessage(QString("Stop debug result: %1").arg(result));
	}

	void executeCleanProject()
	{
		outputMessage("Cleaning project...");
		bool success = m_commandsP->cleanProject();
		QString result = success ? QStringLiteral("Project clean started successfully") : QStringLiteral("Project clean failed to start");
		outputMessage(QString("Clean result: %1").arg(result));
	}

	void executeSaveSession()
	{
		outputMessage("Saving session...");
		bool success = m_commandsP->saveSession();
		QString result = success ? QStringLiteral("Session saved successfully") : QStringLiteral("Session save failed");
		outputMessage(QString("Save session result: %1").arg(result));
	}

	void executeQuit()
	{
		outputMessage("Quitting Qt Creator...");
		bool success = m_commandsP->quit();
		QString result = success ? QStringLiteral("Quit initiated successfully") : QStringLiteral("Quit failed");
		outputMessage(QString("Quit result: %1").arg(result));
	}

	MCPServer *m_serverP = nullptr;
	MCPCommands *m_commandsP = nullptr;
};

} // namespace Qt_MCP_Plugin::Internal

#include "qt_mcp_plugin.moc"
