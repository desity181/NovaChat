#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QStandardPaths>
#include "logger/Logger.h"
#include "config/AppConfig.h"
#include "storage/DatabaseManager.h"
#include "storage/UserRepository.h"
#include "storage/ContactRepository.h"
#include "storage/MessageRepository.h"
#include "storage/ConversationRepository.h"
#include "network/TcpClient.h"
#include "service/AuthService.h"
#include "service/ContactService.h"
#include "service/ChatService.h"
#include "ui/AppStyle.h"
#include "ui/LoginWindow.h"
#include "ui/MainWindow.h"

// Single-instance guard scoped to a named profile.
// Returns false if another instance with the same profile is already running.
static bool acquireSingleInstanceLock(const QString& profile,
                                       QLocalServer*& server) {
    const QString kServerName = "NovaChatInstance_" + profile;

    QLocalSocket probe;
    probe.connectToServer(kServerName);
    if (probe.waitForConnected(200)) {
        return false;
    }

    server = new QLocalServer();
    QLocalServer::removeServer(kServerName);
    server->listen(kServerName);
    return true;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("NovaChat");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("NovaChat");

    // ── Command-line: --profile <name> ──────────────────────────
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption profileOpt(
        QStringList{"p", "profile"},
        "Run as a named profile (allows multiple instances).\n"
        "Each profile has its own database and login state.\n"
        "Example: --profile alice",
        "name", "default");
    parser.addOption(profileOpt);
    parser.process(app);

    const QString profile = parser.value(profileOpt).trimmed().isEmpty()
                            ? QStringLiteral("default")
                            : parser.value(profileOpt).trimmed();

    // ── Style ────────────────────────────────────────────────────
    app.setStyle("Fusion");
    const QString qss = AppStyle::loadStyleSheet();
    if (!qss.isEmpty()) app.setStyleSheet(qss);

    // ── Single-instance check (per profile) ─────────────────────
    QLocalServer* instance_server = nullptr;
    if (!acquireSingleInstanceLock(profile, instance_server)) {
        QMessageBox::information(nullptr, "NovaChat",
            QString("A NovaChat instance with profile \"%1\" is already running.\n\n"
                    "Use --profile <name> to start a second instance.\n"
                    "Example:  NovaChat.exe --profile bob").arg(profile));
        return 0;
    }

    // ── Logging (per profile) ────────────────────────────────────
    const std::string log_file = profile == "default"
        ? "novachat.log"
        : "novachat_" + profile.toStdString() + ".log";
    Logger::init(log_file);
    Logger::info("NovaChat v0.1.0 starting (profile: {})", profile.toStdString());

    // ── Config ──────────────────────────────────────────────────
    AppConfig::instance().load("config.yaml");

    // ── Database (per profile) ───────────────────────────────────
    const QString data_dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(data_dir);
    const QString db_filename = profile == "default"
        ? "novachat.db"
        : "novachat_" + profile + ".db";
    const std::string db_path = (data_dir + "/" + db_filename).toStdString();

    std::unique_ptr<DatabaseManager>        db;
    std::unique_ptr<UserRepository>         user_repo;
    std::unique_ptr<ContactRepository>      contact_repo;
    std::unique_ptr<MessageRepository>      msg_repo;
    std::unique_ptr<ConversationRepository> conv_repo;

    try {
        db           = std::make_unique<DatabaseManager>(db_path);
        user_repo    = std::make_unique<UserRepository>(*db);
        contact_repo = std::make_unique<ContactRepository>(*db);
        msg_repo     = std::make_unique<MessageRepository>(*db);
        conv_repo    = std::make_unique<ConversationRepository>(*db);
    } catch (const DatabaseException& e) {
        QMessageBox::critical(nullptr, "Database Error",
            QString("Failed to open database for profile \"%1\":\n%2")
            .arg(profile, QString::fromStdString(e.what())));
        return 1;
    }

    // ── Network ─────────────────────────────────────────────────
    auto* tcp_client = new TcpClient(&app);

    auto launch_main = [&](const User& user) {
        auto* auth_svc    = new AuthService(tcp_client, *user_repo, &app);
        auto* contact_svc = new ContactService(tcp_client, *contact_repo, user, &app);
        auto* chat_svc    = new ChatService(tcp_client, *msg_repo, *conv_repo, user, &app);

        auto* win = new MainWindow(user, tcp_client, auth_svc, contact_svc, chat_svc);
        win->show();

        QObject::connect(tcp_client, &TcpClient::connected,
                         contact_svc, &ContactService::syncContacts);
        QObject::connect(tcp_client, &TcpClient::connected,
                         chat_svc,   &ChatService::syncConversations);

        auto handle_auth_expired = [auth_svc, user_repo_ptr = user_repo.get(),
                                     tcp_client, win]() {
            Logger::warn("Token expired — clearing and returning to login");
            user_repo_ptr->clearActive();
            tcp_client->disconnectFromServer();
            auto* login_win = new LoginWindow(auth_svc);
            login_win->show();
            win->close();
        };
        QObject::connect(contact_svc, &ContactService::authExpired,
                         &app, handle_auth_expired, Qt::QueuedConnection);
        QObject::connect(chat_svc,    &ChatService::authExpired,
                         &app, handle_auth_expired, Qt::QueuedConnection);
    };

    // ── Startup window selection ─────────────────────────────────
    auto active = user_repo->findActive();

    if (active) {
        Logger::info("Auto-login: {}", active->username);
        User user;
        user.server_id    = active->server_id;
        user.username     = active->username;
        user.display_name = active->display_name;
        user.avatar_url   = active->avatar_url;
        user.token        = active->token;
        launch_main(user);
    } else {
        auto* auth_svc  = new AuthService(tcp_client, *user_repo, &app);
        auto* login_win = new LoginWindow(auth_svc);
        QObject::connect(login_win, &LoginWindow::loginSucceeded,
                         [&launch_main](User user) { launch_main(user); });
        login_win->show();
    }

    tcp_client->connectToServer(AppConfig::instance().server.host,
                                 AppConfig::instance().server.port);

    Logger::info("Event loop starting");
    const int result = app.exec();

    delete instance_server;
    Logger::info("NovaChat exiting");
    Logger::shutdown();
    return result;
}
