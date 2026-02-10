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
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QDebug>
#include <QLoggingCategory>
#include <QTimer>

// Cross-platform menu icon support (handles macOS quirks automatically)
#include "macos_menu_icon.h"
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

// Define logging category for the plugin
Q_LOGGING_CATEGORY(mcpPlugin, "qtcreator.mcpplugin", QtWarningMsg)

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
		
		// Title with icon
		QHBoxLayout *titleLayout = new QHBoxLayout();
		
		QLabel *iconLabel = new QLabel();
		QIcon mcpIcon(":/icons/mcp.png");
		QPixmap iconPixmap = mcpIcon.pixmap(48, 48);
		iconLabel->setPixmap(iconPixmap);
		titleLayout->addWidget(iconLabel);
		
		QLabel *titleLabel = new QLabel(QString("Qt MCP Plugin v%1").arg(PLUGIN_VERSION_STRING));
		QFont titleFont = titleLabel->font();
		titleFont.setPointSize(14);
		titleFont.setBold(true);
		titleLabel->setFont(titleFont);
		titleLayout->addWidget(titleLabel);
		titleLayout->addStretch();
		
		layout->addLayout(titleLayout);
		
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
		qCDebug(mcpPlugin) << "Qt MCP Plugin initializing...";
		qCDebug(mcpPlugin) << "Plugin version:" << PLUGIN_VERSION_STRING;
		qCDebug(mcpPlugin) << "Qt version:" << QT_VERSION_STR;
		qCDebug(mcpPlugin) << "Build type:" << 
#ifdef QT_NO_DEBUG
			"Release"
#else
			"Debug"
#endif
			;
		
		// Create the MCP server and commands
		qCDebug(mcpPlugin) << "Creating MCP server and commands...";
		m_serverP = new MCPServer(this);
		m_commandsP = new MCPCommands(this);

		// Initialize the server
		qCDebug(mcpPlugin) << "Starting MCP server...";
		if (!m_serverP->start()) {
			qCCritical(mcpPlugin) << "Failed to start MCP server";
			QMessageBox::warning(ICore::dialogParent(),
							   Tr::tr("MCP Plugin"),
							   Tr::tr("Failed to start MCP server"));
		} else {
			qCInfo(mcpPlugin) << "MCP server started successfully on port" << m_serverP->getPort();
			// Show startup message in General Messages panel
			outputMessage(QString("MCP Plugin v%1 loaded and functioning - MCP server running on port %2")
				.arg(PLUGIN_VERSION_STRING)
				.arg(m_serverP->getPort()));
		}

		// Create the MCP Plugin menu with icon
		ActionContainer *menu = ActionManager::createMenu(Constants::MENU_ID);
		menu->menu()->setTitle(Tr::tr("MCP Plugin"));
		
		// Set menu icon BEFORE adding to parent (may matter for macOS)
		setupMenuIcon(menu->menu(), "MCP Plugin");
		
		// Now add to Tools menu
		ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

		// Add separator for About
		menu->addSeparator();

		// About action (shows the existing status dialog)
		ActionBuilder(this, Constants::ABOUT_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(QString("ℹ️ About MCP Plugin v%1").arg(PLUGIN_VERSION_STRING))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::showAbout);

		// Add separator between About and commands
		menu->addSeparator();

		// MCP Protocol Discovery section
		ActionBuilder(this, Constants::MCP_INITIALIZE_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("🚀 MCP: Initialize"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeMCPInitialize);

		ActionBuilder(this, Constants::MCP_TOOLS_LIST_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("🛠️ MCP: List Tools"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeMCPToolsList);

		menu->addSeparator();

		// Project Information section
		ActionBuilder(this, Constants::LIST_SESSIONS_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("📁 List Sessions"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeListSessions);

		ActionBuilder(this, Constants::LIST_PROJECTS_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("📂 List Projects"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeListProjects);

		ActionBuilder(this, Constants::LIST_BUILD_CONFIGS_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("⚙️ List Build Configs"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeListBuildConfigs);

		ActionBuilder(this, Constants::GET_CURRENT_PROJECT_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("📋 Get Current Project"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeGetCurrentProject);

		ActionBuilder(this, Constants::GET_CURRENT_BUILD_CONFIG_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("🔧 Get Current Build Config"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeGetCurrentBuildConfig);

		ActionBuilder(this, Constants::GET_CURRENT_SESSION_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("🎯 Get Current Session"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeGetCurrentSession);

		ActionBuilder(this, Constants::LIST_OPEN_FILES_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("📄 List Open Files"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeListOpenFiles);

		ActionBuilder(this, Constants::LIST_ISSUES_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("⚠️ List Issues"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeListIssues);

		ActionBuilder(this, Constants::GET_BUILD_DIAGNOSTICS_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("📋 Get Build Diagnostics (JSON)"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeGetBuildDiagnostics);

		ActionBuilder(this, Constants::GET_METHOD_METADATA_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("📊 Get Method Metadata"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeGetMethodMetadata);

		ActionBuilder(this, Constants::SET_METHOD_METADATA_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("⚡ Set Method Metadata"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeSetMethodMetadata);

		menu->addSeparator();

		ActionBuilder(this, Constants::BUILD_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("🔨 Build Project"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeBuild);

		ActionBuilder(this, Constants::RUN_PROJECT_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("▶️ Run Project"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeRunProject);

		ActionBuilder(this, Constants::DEBUG_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("🐛 Debug Project"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeDebug);

		ActionBuilder(this, Constants::STOP_DEBUG_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("⏹️ Stop Debugging"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeStopDebug);

		ActionBuilder(this, Constants::CLEAN_PROJECT_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("🧹 Clean Project"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeCleanProject);

		ActionBuilder(this, Constants::SAVE_SESSION_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("💾 Save Session"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::executeSaveSession);

		menu->addSeparator();

		ActionBuilder(this, Constants::QUIT_ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("🚪 Quit Qt Creator"))
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

	/**
	 * Set up the menu icon for the MCP Plugin menu.
	 * 
	 * This function handles platform differences:
	 * - Windows/Linux: Qt's icon methods work directly
	 * - macOS: Qt may not show icons due to Apple HIG; uses native fallback
	 * 
	 * @param menu The QMenu to set the icon on
	 * @param menuTitle The title used to find the menu in native APIs
	 */
	void setupMenuIcon(QMenu *menu, const QString &menuTitle)
	{
		if (!menu)
			return;
		
		QIcon mcpIcon(":/icons/mcp.png");
		if (mcpIcon.isNull())
			return;
		
		// Set icon via Qt's standard methods
		QAction *menuAction = menu->menuAction();
		if (menuAction) {
			menuAction->setIcon(mcpIcon);
			menuAction->setIconVisibleInMenu(true);
		}
		menu->setIcon(mcpIcon);
		
		// Cross-platform fallback (no-op on Windows/Linux, native API on macOS)
		setMenuIconWithFallback(menu, mcpIcon, menuTitle);
	}

	void executeMCPInitialize()
	{
		if (!m_serverP || !m_serverP->isRunning()) {
			outputMessage("❌ MCP Server is not running. Please start Qt Creator first.");
			return;
		}
		
		// Create JSON-RPC request for MCP initialize
		QJsonObject request;
		request["jsonrpc"] = "2.0";
		request["method"] = "initialize";
		request["id"] = 1;
		
		QJsonObject params;
		params["protocolVersion"] = "2024-11-05";
		params["capabilities"] = QJsonObject();
		
		QJsonObject clientInfo;
		clientInfo["name"] = "Qt Creator";
		clientInfo["version"] = "1.0.0";
		params["clientInfo"] = clientInfo;
		
		request["params"] = params;
		
		QJsonDocument doc(request);
		QString requestJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
		outputMessage(QString("MCP Initialize Request: %1").arg(requestJson));
		
		// Actually call the MCP server and get the real response
		QJsonObject response = m_serverP->callMCPMethod("initialize", params);
		
		if (response.contains("error")) {
			outputMessage(QString("❌ MCP Error: %1").arg(response["error"].toObject()["message"].toString()));
		} else if (response.contains("result")) {
			QJsonObject result = response["result"].toObject();
			outputMessage("✅ MCP Initialize successful!");
			
			if (result.contains("protocolVersion")) {
				outputMessage(QString("  Protocol Version: %1").arg(result["protocolVersion"].toString()));
			}
			if (result.contains("serverInfo")) {
				QJsonObject serverInfo = result["serverInfo"].toObject();
				if (serverInfo.contains("name")) {
					outputMessage(QString("  Server Name: %1").arg(serverInfo["name"].toString()));
				}
				if (serverInfo.contains("version")) {
					outputMessage(QString("  Server Version: %1").arg(serverInfo["version"].toString()));
				}
			}
			if (result.contains("capabilities")) {
				outputMessage(QString("  Capabilities: %1").arg(QJsonDocument(result["capabilities"].toObject()).toJson(QJsonDocument::Compact)));
			}
		} else {
			outputMessage("❌ No result in MCP response");
		}
	}

	void executeMCPToolsList()
	{
		if (!m_serverP || !m_serverP->isRunning()) {
			outputMessage("❌ MCP Server is not running. Please start Qt Creator first.");
			return;
		}
		
		// Create JSON-RPC request for MCP tools/list
		QJsonObject request;
		request["jsonrpc"] = "2.0";
		request["method"] = "tools/list";
		request["id"] = 1;
		
		QJsonDocument doc(request);
		QString requestJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
		outputMessage(QString("MCP Tools List Request: %1").arg(requestJson));
		
		// Actually call the MCP server and get the real response
		QJsonObject response = m_serverP->callMCPMethod("tools/list", QJsonValue());
		
		if (response.contains("error")) {
			outputMessage(QString("❌ MCP Error: %1").arg(response["error"].toObject()["message"].toString()));
		} else if (response.contains("result")) {
			QJsonObject result = response["result"].toObject();
			if (result.contains("tools")) {
				QJsonArray tools = result["tools"].toArray();
				outputMessage(QString("✅ Found %1 MCP tools:").arg(tools.size()));
				
				for (const QJsonValue &toolValue : tools) {
					QJsonObject tool = toolValue.toObject();
					QString name = tool["name"].toString();
					QString description = tool["description"].toString();
					outputMessage(QString("  • %1: %2").arg(name, description));
				}
			} else {
				outputMessage("❌ Invalid MCP response format");
			}
		} else {
			outputMessage("❌ No result in MCP response");
		}
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

	void executeGetBuildDiagnostics()
	{
		QString json = m_commandsP->getBuildDiagnostics(QStringLiteral("all"));
		outputMessage(QString("Build Diagnostics (JSON): %1").arg(json));
	}

		void executeGetMethodMetadata()
	{
		outputMessage("Getting method metadata...");
		// Get metadata for all methods
		QString result = m_commandsP->getMethodMetadata();
		outputMessage(result);
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
