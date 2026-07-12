#include "ui/MainWindow.h"
#include "ui/LoginWindow.h"
#include "ui/SettingsDialog.h"
#include <QApplication>
#include <QMenuBar>
#include <QSplitter>
#include <QStatusBar>

MainWindow::MainWindow(const User&     user,
                        TcpClient*      tcp_client,
                        AuthService*    auth_service,
                        ContactService* contact_service,
                        ChatService*    chat_service,
                        QWidget*        parent)
    : QMainWindow(parent)
    , current_user_(user)
    , auth_service_(auth_service)
    , contact_service_(contact_service)
    , chat_service_(chat_service)
{
    setWindowTitle(QString("NovaChat — %1")
                   .arg(QString::fromStdString(user.display_name)));
    setMinimumSize(1100, 680);

    setupMenuBar();
    setupStatusBar(tcp_client);

    // ── Left panel: Chats | Contacts tabs ─────────────────────
    conv_list_     = new ConversationList(chat_service_, this);
    contact_panel_ = new ContactPanel(contact_service_, this);

    left_tabs_ = new QTabWidget(this);
    left_tabs_->addTab(conv_list_,     "Chats");
    left_tabs_->addTab(contact_panel_, "Contacts");
    left_tabs_->setFixedWidth(300);

    // ── Right panel: ChatView ──────────────────────────────────
    chat_view_ = new ChatView(
        chat_service_,
        QString::fromStdString(user.server_id),
        this);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(left_tabs_);
    splitter->addWidget(chat_view_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    connect(conv_list_,    &ConversationList::conversationSelected,
            this,          &MainWindow::onConversationSelected);
    connect(contact_panel_, &ContactPanel::contactSelected,
            this,           &MainWindow::onContactSelected);

    connectServiceErrors();
}

void MainWindow::setupMenuBar() {
    auto* file_menu    = menuBar()->addMenu("File");
    auto* settings_act = file_menu->addAction("Settings");
    auto* logout_act   = file_menu->addAction("Log out");
    file_menu->addSeparator();
    auto* quit_act     = file_menu->addAction("Quit");
    connect(settings_act, &QAction::triggered, this, &MainWindow::onOpenSettings);
    connect(logout_act,   &QAction::triggered, this, &MainWindow::onLogout);
    connect(quit_act,     &QAction::triggered, qApp, &QApplication::quit);
}

void MainWindow::setupStatusBar(TcpClient* tcp_client) {
    conn_indicator_ = new QLabel("Connecting...", this);
    statusBar()->addPermanentWidget(conn_indicator_);
    statusBar()->showMessage(
        QString("Logged in as: %1")
        .arg(QString::fromStdString(current_user_.username)));

    connect(tcp_client, &TcpClient::connected,
            this, &MainWindow::onConnected,       Qt::QueuedConnection);
    connect(tcp_client, &TcpClient::disconnected,
            this, &MainWindow::onDisconnected,    Qt::QueuedConnection);
    connect(tcp_client, &TcpClient::reconnecting,
            this, &MainWindow::onReconnecting,    Qt::QueuedConnection);
    connect(tcp_client, &TcpClient::connectionFailed,
            this, &MainWindow::onConnectionFailed, Qt::QueuedConnection);
}

void MainWindow::connectServiceErrors() {
    connect(contact_service_, &ContactService::operationFailed,
            this, [this](const QString& msg) { showError(msg); });
    connect(chat_service_,    &ChatService::messageFailed,
            this, [this](const std::string&) {
                showError("Message failed to send — check your connection");
            });
}

void MainWindow::showError(const QString& msg) {
    statusBar()->showMessage("⚠  " + msg, 4000);
}

void MainWindow::showInfo(const QString& msg) {
    statusBar()->showMessage(msg, 2000);
}

void MainWindow::onConnected() {
    conn_indicator_->setText("● Connected");
    conn_indicator_->setStyleSheet("color: #07C160;");
    showInfo("Connection restored");
}

void MainWindow::onDisconnected() {
    conn_indicator_->setText("● Disconnected");
    conn_indicator_->setStyleSheet("color: #E53E3E;");
    showError("Connection lost");
}

void MainWindow::onReconnecting(int attempt, int delay_s) {
    conn_indicator_->setText(
        QString("○ Reconnecting (%1s)...").arg(delay_s));
    conn_indicator_->setStyleSheet("color: #D69E2E;");
    showInfo(QString("Reconnecting… attempt %1, retrying in %2s")
             .arg(attempt).arg(delay_s));
}

void MainWindow::onConnectionFailed() {
    conn_indicator_->setText("● Offline");
    conn_indicator_->setStyleSheet("color: #E53E3E;");
    showError("Unable to connect — restart the application to try again");
}

void MainWindow::onLogout() {
    auth_service_->logout();
    auto* login_win = new LoginWindow(auth_service_);
    login_win->show();
    close();
}

void MainWindow::onOpenSettings() {
    auto* dlg = new SettingsDialog(current_user_, auth_service_, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &SettingsDialog::logoutRequested,
            this, &MainWindow::onLogout);
    dlg->exec();
}

void MainWindow::onConversationSelected(const std::string& conv_id,
                                         const std::string& peer_user_id) {
    chat_view_->openConversation(conv_id, peer_user_id);
    chat_service_->clearUnread(conv_id);
    left_tabs_->setCurrentIndex(0);
}

void MainWindow::onContactSelected(const Contact& contact) {
    const std::string conv_id =
        ChatService::makeConvId(current_user_.server_id, contact.user_id);
    chat_view_->openConversation(conv_id, contact.user_id);
    left_tabs_->setCurrentIndex(0);
}
