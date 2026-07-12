#pragma once
#include "core/Contact.h"
#include "core/User.h"
#include "network/TcpClient.h"
#include "service/AuthService.h"
#include "service/ChatService.h"
#include "service/ContactService.h"
#include "ui/ChatView.h"
#include "ui/ContactPanel.h"
#include "ui/ConversationList.h"
#include <QLabel>
#include <QMainWindow>
#include <QSplitter>
#include <QTabWidget>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const User&     user,
                         TcpClient*      tcp_client,
                         AuthService*    auth_service,
                         ContactService* contact_service,
                         ChatService*    chat_service,
                         QWidget*        parent = nullptr);
    ~MainWindow() override = default;

    void showError(const QString& msg);
    void showInfo (const QString& msg);

private slots:
    void onLogout();
    void onConversationSelected(const std::string& conv_id,
                                 const std::string& peer_user_id);
    void onContactSelected(const Contact& contact);
    void onOpenSettings();

    void onConnected      ();
    void onDisconnected   ();
    void onReconnecting   (int attempt, int delay_s);
    void onConnectionFailed();

private:
    void setupStatusBar(TcpClient* tcp_client);
    void setupMenuBar  ();
    void connectServiceErrors();

    User            current_user_;
    AuthService*    auth_service_;
    ContactService* contact_service_;
    ChatService*    chat_service_;

    QTabWidget*       left_tabs_;
    ConversationList* conv_list_;
    ContactPanel*     contact_panel_;
    ChatView*         chat_view_;

    QLabel*           conn_indicator_;
};
