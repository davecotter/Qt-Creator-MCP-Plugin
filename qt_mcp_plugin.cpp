#include "qt_mcp_pluginconstants.h"
#include "qt_mcp_plugintr.h"
#include "mcpserver.h"
#include "version.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>

#include <extensionsystem/iplugin.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QPalette>

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
		setWindowTitle(Tr::tr("MCP Plugin Status"));
		setModal(true);
		resize(400, 200);
		
		QVBoxLayout *layout = new QVBoxLayout(this);
		
		// Title
		QLabel *titleLabel = new QLabel(Tr::tr("Qt MCP Plugin"));
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
	}

	void initialize() final
	{
		// Create the MCP server
		m_serverP = new MCPServer(this);

		// Initialize the server
		if (!m_serverP->start()) {
			QMessageBox::warning(ICore::dialogParent(),
							   Tr::tr("MCP Plugin"),
							   Tr::tr("Failed to start MCP server"));
		}

		ActionContainer *menu = ActionManager::createMenu(Constants::MENU_ID);
		menu->menu()->setTitle(Tr::tr("MCP Plugin"));
		ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

		ActionBuilder(this, Constants::ACTION_ID)
				.addToContainer(Constants::MENU_ID)
				.setText(Tr::tr("MCP Server Status"))
				.setDefaultKeySequence(Tr::tr("Ctrl+Alt+Meta+M"))
				.addOnTriggered(this, &Qt_MCP_PluginPlugin::triggerAction);
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
	void triggerAction()
	{
		MCPServerStatusDialog dialog(m_serverP, ICore::dialogParent());
		dialog.exec();
	}

	MCPServer *m_serverP = nullptr;
};

} // namespace Qt_MCP_Plugin::Internal

#include "qt_mcp_plugin.moc"
