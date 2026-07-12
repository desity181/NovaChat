# NovaChat V1 — 阶段六实现计划：完善收尾

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 补全 V1 剩余能力：TcpClient 断线自动重连（指数退避）；AUTH_EXPIRED 统一捕获并强制返回登录界面；统一错误通知展示（状态栏 + 连接指示灯）；SettingsDialog 账号信息面板；全局 QSS 样式统一；完整集成验证（注册→登录→加好友→聊天→重启恢复）。

**Architecture:** 重连逻辑封装在 TcpClient 内，外部只感知 `connected/disconnected/reconnecting` 信号。AUTH_EXPIRED 错误由各 Service 捕获后发出 `authExpired` 信号，冒泡至 `main.cpp` 统一处理，清除本地 token 并跳回 LoginWindow。错误通知通过 MainWindow 状态栏集中展示，无需 QMessageBox 中断用户操作流。

**Tech Stack:** C++20 · Qt6 Widgets · Asio steady_timer（重连）

---

## 本阶段新增/修改文件

```
NovaChat/
├── client/
│   ├── CMakeLists.txt                  ← 修改：追加 UI 文件
│   └── src/
│       ├── main.cpp                    ← 修改：AUTH_EXPIRED 强制登出
│       ├── network/
│       │   ├── TcpClient.h             ← 修改：重连信号 + disconnect()
│       │   └── TcpClient.cpp           ← 修改：重连逻辑
│       ├── service/
│       │   ├── ContactService.h/cpp    ← 修改：authExpired 信号
│       │   └── ChatService.h/cpp       ← 修改：authExpired 信号
│       └── ui/
│           ├── MainWindow.h/cpp        ← 修改：连接状态栏 + 错误通知
│           ├── SettingsDialog.h        ← 新增
│           ├── SettingsDialog.cpp      ← 新增
│           └── AppStyle.h             ← 新增：样式常量
│   ├── resources/
│   │   └── style.qss                  ← 新增：全局样式表
└── docs/
    └── superpowers/plans/
        └── integration-checklist.md   ← 新增：集成验证清单
```

---

## Task 29：TcpClient 断线自动重连

**Files:**
- Modify: `client/src/network/TcpClient.h`
- Modify: `client/src/network/TcpClient.cpp`

- [ ] **Step 1：在 client/src/network/TcpClient.h 中添加重连相关声明**

在 `public:` 区块追加：
```cpp
    // 主动断开（不触发重连）
    void disconnectFromServer();

signals:
    // 已有信号（确认存在）：
    void connected();
    void disconnected();

    // 新增信号：
    // 正在等待重连，attempt 从 1 计，delay_seconds 为等待秒数
    void reconnecting(int attempt, int delay_seconds);

    // 超过最大重试次数后放弃
    void connectionFailed();
```

在 `private:` 区块追加：
```cpp
    void doConnect();          // 封装实际连接动作，初次和重连复用
    void scheduleReconnect(); // 按指数退避延迟后调用 doConnect
    void onConnectionLost();  // 连接中断时的统一处理

    asio::steady_timer reconnect_timer_;   // 重连定时器
    std::string cached_host_;
    uint16_t    cached_port_{0};
    int         reconnect_attempt_{0};
    bool        manual_disconnect_{false};

    static constexpr int kMaxReconnectAttempts = 8;
    static constexpr int kMaxReconnectDelaySec = 60;
```

同时将构造函数中初始化 `io_context_` 的初始化列表补上 `reconnect_timer_`：
```cpp
TcpClient::TcpClient(QObject* parent)
    : QObject(parent)
    , io_context_()
    , reconnect_timer_(io_context_)   // ← 追加
    , ...
```

- [ ] **Step 2：重构 TcpClient.cpp**

将原有 `connectToServer` 的连接代码提取到 `doConnect()`，`connectToServer` 只做参数缓存 + 调用 `doConnect`：

```cpp
void TcpClient::connectToServer(const std::string& host, uint16_t port) {
    cached_host_     = host;
    cached_port_     = port;
    manual_disconnect_ = false;
    reconnect_attempt_ = 0;
    doConnect();
}

void TcpClient::disconnectFromServer() {
    manual_disconnect_ = true;
    asio::post(io_context_, [this] {
        asio::error_code ec;
        socket_.close(ec);
        reconnect_timer_.cancel();
    });
}
```

添加 `doConnect()` 实现（替换原有连接代码，使用缓存的 host/port）：
```cpp
void TcpClient::doConnect() {
    asio::post(io_context_, [this] {
        asio::error_code ec;
        socket_.close(ec);

        asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(cached_host_,
                                          std::to_string(cached_port_), ec);
        if (ec) { scheduleReconnect(); return; }

        asio::async_connect(socket_, endpoints,
            [this](const asio::error_code& err,
                   const asio::ip::tcp::endpoint&) {
                if (err) {
                    scheduleReconnect();
                    return;
                }
                reconnect_attempt_ = 0;
                {
                    std::lock_guard<std::mutex> lock(callbacks_mutex_);
                    callbacks_.clear();   // 清除旧的挂起回调
                }
                emit connected();
                startRead();   // 开始异步读取循环
            });
    });
}
```

添加 `scheduleReconnect()` 实现：
```cpp
void TcpClient::scheduleReconnect() {
    if (manual_disconnect_) return;
    emit disconnected();

    if (++reconnect_attempt_ > kMaxReconnectAttempts) {
        emit connectionFailed();
        return;
    }

    // 指数退避：2, 4, 8, 16, 32, 60, 60, 60 秒
    const int delay_s = std::min(1 << reconnect_attempt_, kMaxReconnectDelaySec);
    emit reconnecting(reconnect_attempt_, delay_s);

    reconnect_timer_.expires_after(std::chrono::seconds(delay_s));
    reconnect_timer_.async_wait([this](const asio::error_code& ec) {
        if (!ec && !manual_disconnect_) doConnect();
    });
}
```

添加 `onConnectionLost()` — 在异步读取回调中，当 `error_code` 非零时调用：
```cpp
void TcpClient::onConnectionLost() {
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        callbacks_.clear();
    }
    scheduleReconnect();
}
```

确认异步读取循环中的错误处理路径调用了 `onConnectionLost()`（而非直接 return）：
```cpp
// 在 handleRead 或 startRead 的 lambda 中:
if (ec) {
    onConnectionLost();   // ← 替换原有的简单 return
    return;
}
```

- [ ] **Step 3：编译验证**

```
cmake --build --preset windows-debug
```

预期：编译无报错。运行服务端，启动客户端，然后关闭服务端——客户端状态栏（下一步实现）会显示重连信息；重启服务端后客户端自动重连。

- [ ] **Step 4：提交**

```
git add client/src/network/TcpClient.h client/src/network/TcpClient.cpp
git commit -m "feat(network): TcpClient 支持断线自动重连（指数退避，最多 8 次）"
```

---

## Task 30：AUTH_EXPIRED 统一处理 + 错误通知集成

**Files:**
- Modify: `client/src/service/ContactService.h/cpp`
- Modify: `client/src/service/ChatService.h/cpp`
- Modify: `client/src/ui/MainWindow.h/cpp`
- Modify: `client/src/main.cpp`

- [ ] **Step 1：在 ContactService.h 添加 authExpired 信号**

```cpp
signals:
    void contactsSynced (std::vector<Contact> contacts);
    void contactAdded   (Contact contact);
    void contactDeleted (std::string user_id);
    void searchResult   (std::vector<Contact> users);
    void operationFailed(const QString& reason);
    void authExpired    ();   // ← 新增
```

- [ ] **Step 2：在 ContactService.cpp 的错误处理分支检测 AUTH_EXPIRED**

在所有 `operationFailed` 的分支前，添加检测（以 `syncContacts` 的回调为例，其余回调同理）：

```cpp
} else if (resp.type == MessageType::ErrorResponse) {
    novachat::ErrorResponse er;
    er.ParseFromArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    if (er.code() == novachat::AUTH_EXPIRED) {
        emit authExpired();
    } else {
        emit operationFailed(QString::fromStdString(er.message()));
    }
}
```

对 `addFriend`、`deleteFriend`、`searchUser` 的回调分支执行相同修改。

- [ ] **Step 3：在 ChatService.h 添加 authExpired 信号**

```cpp
signals:
    void messageSent       (Message message);
    void messageFailed     (std::string local_id);
    void messageReceived   (Message message);
    void historyLoaded     (std::string conv_id, std::vector<Message> messages, bool has_more);
    void conversationsLoaded(std::vector<Conversation> conversations);
    void authExpired       ();   // ← 新增
```

- [ ] **Step 4：在 ChatService.cpp 的错误处理分支检测 AUTH_EXPIRED**

在 `sendMessage`、`loadHistory`、`syncConversations` 回调中，遇到 `ErrorResponse` 时：

```cpp
} else if (resp.type == MessageType::ErrorResponse) {
    novachat::ErrorResponse er;
    er.ParseFromArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    if (er.code() == novachat::AUTH_EXPIRED) {
        emit authExpired();
    }
    // sendMessage 失败时额外通知
    // （在 sendMessage 的回调中，失败分支同时调用 messageFailed）
}
```

- [ ] **Step 5：更新 client/src/ui/MainWindow.h — 添加连接状态指示 + 错误通知支持**

```cpp
#pragma once
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
    explicit MainWindow(const User&      user,
                         TcpClient*       tcp_client,
                         AuthService*     auth_service,
                         ContactService*  contact_service,
                         ChatService*     chat_service,
                         QWidget*         parent = nullptr);
    ~MainWindow() override = default;

    // 显示瞬时通知（3s 后自动消失）
    void showError  (const QString& msg);
    void showInfo   (const QString& msg);

private slots:
    void onLogout();
    void onConversationSelected(const std::string& conv_id,
                                 const std::string& peer_user_id);
    void onContactSelected(const Contact& contact);
    void onOpenSettings();

    void onConnected   ();
    void onDisconnected();
    void onReconnecting(int attempt, int delay_s);
    void onConnectionFailed();

private:
    void setupStatusBar(TcpClient* tcp_client);
    void setupMenuBar  ();
    void connectServiceErrors();

    User             current_user_;
    AuthService*     auth_service_;
    ContactService*  contact_service_;
    ChatService*     chat_service_;

    QTabWidget*       left_tabs_;
    ConversationList* conv_list_;
    ContactPanel*     contact_panel_;
    ChatView*         chat_view_;

    QLabel*          conn_indicator_;   // 状态栏连接指示灯
};
```

- [ ] **Step 6：更新 client/src/ui/MainWindow.cpp**

```cpp
#include "ui/MainWindow.h"
#include "ui/LoginWindow.h"
#include "ui/SettingsDialog.h"
#include <QApplication>
#include <QMenuBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>

MainWindow::MainWindow(const User&      user,
                        TcpClient*       tcp_client,
                        AuthService*     auth_service,
                        ContactService*  contact_service,
                        ChatService*     chat_service,
                        QWidget*         parent)
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

    // ── 左侧 Tab ─────────────────────────────────────────────────
    conv_list_    = new ConversationList(chat_service_, this);
    contact_panel_ = new ContactPanel(contact_service_, this);

    left_tabs_ = new QTabWidget(this);
    left_tabs_->addTab(conv_list_,     "💬 会话");
    left_tabs_->addTab(contact_panel_, "👥 联系人");
    left_tabs_->setFixedWidth(300);

    // ── 右侧 ChatView ─────────────────────────────────────────────
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

    // ── 信号连接 ─────────────────────────────────────────────────
    connect(conv_list_,    &ConversationList::conversationSelected,
            this,          &MainWindow::onConversationSelected);
    connect(contact_panel_, &ContactPanel::contactSelected,
            this,           &MainWindow::onContactSelected);

    connectServiceErrors();
}

void MainWindow::setupMenuBar() {
    auto* file_menu    = menuBar()->addMenu("文件");
    auto* settings_act = file_menu->addAction("设置");
    auto* logout_act   = file_menu->addAction("退出登录");
    file_menu->addSeparator();
    auto* quit_act     = file_menu->addAction("退出程序");
    connect(settings_act, &QAction::triggered, this, &MainWindow::onOpenSettings);
    connect(logout_act,   &QAction::triggered, this, &MainWindow::onLogout);
    connect(quit_act,     &QAction::triggered, qApp, &QApplication::quit);
}

void MainWindow::setupStatusBar(TcpClient* tcp_client) {
    conn_indicator_ = new QLabel("🟡 连接中...", this);
    statusBar()->addPermanentWidget(conn_indicator_);
    statusBar()->showMessage(
        QString("已登录：%1").arg(QString::fromStdString(current_user_.username)));

    connect(tcp_client, &TcpClient::connected,
            this,        &MainWindow::onConnected,     Qt::QueuedConnection);
    connect(tcp_client, &TcpClient::disconnected,
            this,        &MainWindow::onDisconnected,  Qt::QueuedConnection);
    connect(tcp_client, &TcpClient::reconnecting,
            this,        &MainWindow::onReconnecting,  Qt::QueuedConnection);
    connect(tcp_client, &TcpClient::connectionFailed,
            this,        &MainWindow::onConnectionFailed, Qt::QueuedConnection);
}

void MainWindow::connectServiceErrors() {
    connect(contact_service_, &ContactService::operationFailed,
            this, [this](const QString& msg) { showError(msg); });
    connect(chat_service_,    &ChatService::messageFailed,
            this, [this](const std::string&) {
                showError("消息发送失败，请检查网络连接");
            });
}

void MainWindow::showError(const QString& msg) {
    statusBar()->showMessage("⚠  " + msg, 4000);
}

void MainWindow::showInfo(const QString& msg) {
    statusBar()->showMessage("ℹ  " + msg, 2000);
}

void MainWindow::onConnected() {
    conn_indicator_->setText("🟢 已连接");
    showInfo("连接已恢复");
}

void MainWindow::onDisconnected() {
    conn_indicator_->setText("🔴 断开连接");
    showError("连接已断开");
}

void MainWindow::onReconnecting(int attempt, int delay_s) {
    conn_indicator_->setText(
        QString("🟡 重连中 (%1s)...").arg(delay_s));
    showInfo(QString("正在重连… 第 %1 次，%2 秒后重试")
             .arg(attempt).arg(delay_s));
}

void MainWindow::onConnectionFailed() {
    conn_indicator_->setText("🔴 连接失败");
    showError("无法连接到服务器，请检查网络后手动重启");
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
```

- [ ] **Step 7：更新 client/src/main.cpp — 处理 authExpired**

在 `launch_main` lambda 中，为两个服务的 `authExpired` 信号各添加一个连接：

```cpp
auto launch_main = [&](const User& user) -> MainWindow* {
    auto* auth_svc    = new AuthService(tcp_client, *user_repo, &app);
    auto* contact_svc = new ContactService(tcp_client, *contact_repo, user, &app);
    auto* chat_svc    = new ChatService(tcp_client, *msg_repo, *conv_repo, user, &app);

    auto* win = new MainWindow(user, tcp_client, auth_svc, contact_svc, chat_svc);
    win->show();

    // 服务连接后自动同步
    QObject::connect(tcp_client, &TcpClient::connected,
                     contact_svc, &ContactService::syncContacts);
    QObject::connect(tcp_client, &TcpClient::connected,
                     chat_svc,   &ChatService::syncConversations);

    // AUTH_EXPIRED：清除本地 token 并强制返回登录界面
    auto handle_auth_expired = [auth_svc, user_repo_ptr = user_repo.get(),
                                 tcp_client, win] {
        Logger::warn("Token 已过期，清除并返回登录界面");
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

    return win;
};
```

- [ ] **Step 8：编译验证**

```
cmake --build --preset windows-debug
```

预期：编译无报错。

- [ ] **Step 9：提交**

```
git add client/src/service/ContactService.h client/src/service/ContactService.cpp
git add client/src/service/ChatService.h client/src/service/ChatService.cpp
git add client/src/ui/MainWindow.h client/src/ui/MainWindow.cpp
git add client/src/main.cpp
git commit -m "feat: AUTH_EXPIRED 强制登出 + 状态栏连接指示 + 错误通知集成"
```

---

## Task 31：SettingsDialog

**Files:**
- Create: `client/src/ui/SettingsDialog.h`
- Create: `client/src/ui/SettingsDialog.cpp`
- Modify: `client/CMakeLists.txt`

- [ ] **Step 1：写 client/src/ui/SettingsDialog.h**

```cpp
#pragma once
#include "core/User.h"
#include "service/AuthService.h"
#include <QDialog>

// 账号设置对话框
// V1 功能：展示账号信息，提供退出登录入口
// 后续可扩展：修改昵称、头像、通知设置等
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const User& user,
                             AuthService* auth_service,
                             QWidget* parent = nullptr);

signals:
    // 用户点击退出登录时发出（由 MainWindow 处理）
    void logoutRequested();

private:
    AuthService* auth_service_;
};
```

- [ ] **Step 2：写 client/src/ui/SettingsDialog.cpp**

```cpp
#include "ui/SettingsDialog.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const User& user,
                                AuthService* auth_service,
                                QWidget* parent)
    : QDialog(parent)
    , auth_service_(auth_service)
{
    setWindowTitle("账号设置");
    setMinimumWidth(380);
    setModal(true);

    // ── 账号信息（只读）─────────────────────────────────────────
    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);

    auto makeField = [](const std::string& value) -> QLabel* {
        auto* lbl = new QLabel(QString::fromStdString(value));
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        lbl->setStyleSheet("color: #333; padding: 2px 4px;");
        return lbl;
    };

    form->addRow("用户 ID：",  makeField(user.server_id));
    form->addRow("用户名：",   makeField(user.username));
    form->addRow("显示名称：", makeField(user.display_name));
    if (!user.avatar_url.empty()) {
        form->addRow("头像链接：", makeField(user.avatar_url));
    }

    // ── 操作按钮 ─────────────────────────────────────────────────
    auto* logout_btn  = new QPushButton("退出登录", this);
    auto* close_btn   = new QPushButton("关闭",     this);
    logout_btn->setObjectName("dangerButton");   // QSS 可针对此 objectName 设置红色

    auto* btn_row = new QHBoxLayout;
    btn_row->addWidget(logout_btn);
    btn_row->addStretch();
    btn_row->addWidget(close_btn);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);
    layout->addLayout(form);
    layout->addSpacing(8);
    layout->addLayout(btn_row);

    connect(close_btn,  &QPushButton::clicked, this, &QDialog::accept);
    connect(logout_btn, &QPushButton::clicked, this, [this] {
        accept();
        emit logoutRequested();
    });
}
```

- [ ] **Step 3：将 SettingsDialog::logoutRequested 连接到 MainWindow::onLogout**

在 `MainWindow::onOpenSettings()` 中补充连接：

```cpp
void MainWindow::onOpenSettings() {
    auto* dlg = new SettingsDialog(current_user_, auth_service_, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &SettingsDialog::logoutRequested,
            this, &MainWindow::onLogout);
    dlg->exec();
}
```

- [ ] **Step 4：更新 client/CMakeLists.txt，追加 SettingsDialog.cpp 到 NovaChat_UI**

```cmake
src/ui/SettingsDialog.cpp   # ← 追加
```

- [ ] **Step 5：编译验证**

```
cmake --build --preset windows-debug
```

预期：编译无报错。点击「文件 → 设置」弹出对话框，显示账号信息；点击「退出登录」关闭主窗口并显示 LoginWindow。

- [ ] **Step 6：提交**

```
git add client/src/ui/SettingsDialog.h client/src/ui/SettingsDialog.cpp
git add client/CMakeLists.txt
git commit -m "feat(ui): 添加 SettingsDialog，展示账号信息和退出登录"
```

---

## Task 32：全局 QSS 样式

**Files:**
- Create: `client/resources/style.qss`
- Create: `client/src/ui/AppStyle.h`
- Modify: `client/CMakeLists.txt`
- Modify: `client/src/main.cpp`

- [ ] **Step 1：写 client/src/ui/AppStyle.h — 颜色常量**

```cpp
#pragma once
#include <QString>

// 应用颜色系统（集中定义，避免散落在各 .cpp 中）
namespace AppStyle {

    // 主色（发出消息气泡背景）
    inline constexpr auto kColorPrimary    = "#07C160";
    // 气泡文字白色
    inline constexpr auto kColorOnPrimary  = "#FFFFFF";
    // 接收消息气泡背景
    inline constexpr auto kColorBubbleIn   = "#FFFFFF";
    // 聊天区域背景
    inline constexpr auto kColorChatBg     = "#EDEDED";
    // 面板背景（左侧列表）
    inline constexpr auto kColorPanelBg    = "#F7F7F7";
    // 边框颜色
    inline constexpr auto kColorBorder     = "#E0E0E0";
    // 危险操作（退出登录按钮）
    inline constexpr auto kColorDanger     = "#E53E3E";

    // 加载 QSS 文件（resource 路径）
    QString loadStyleSheet();

} // namespace AppStyle
```

- [ ] **Step 2：写 client/resources/style.qss**

```css
/* NovaChat V1 全局样式表 */

QMainWindow, QDialog {
    background-color: #F0F0F0;
    font-family: "Microsoft YaHei", "PingFang SC", "Segoe UI", sans-serif;
    font-size: 13px;
}

/* ── 左侧面板 ─────────────────────────────────────────────────── */
QTabWidget::pane {
    border: none;
    background: #F7F7F7;
}
QTabWidget::tab-bar {
    alignment: left;
}
QTabBar::tab {
    background: #EAEAEA;
    padding: 8px 16px;
    border: none;
    border-bottom: 2px solid transparent;
    font-size: 13px;
    color: #555;
}
QTabBar::tab:selected {
    background: #F7F7F7;
    border-bottom: 2px solid #07C160;
    color: #07C160;
    font-weight: bold;
}
QTabBar::tab:hover:!selected {
    background: #E4E4E4;
}

/* ── 列表 ────────────────────────────────────────────────────── */
QListWidget {
    border: none;
    background: transparent;
    outline: none;
}
QListWidget::item {
    padding: 10px 12px;
    border-bottom: 1px solid #EBEBEB;
    color: #333;
}
QListWidget::item:selected {
    background: #D8F5E4;
    color: #1A7A46;
}
QListWidget::item:hover:!selected {
    background: #EFEFEF;
}

/* ── 输入框 ──────────────────────────────────────────────────── */
QLineEdit {
    border: 1px solid #DCDCDC;
    border-radius: 6px;
    padding: 6px 10px;
    background: #FFFFFF;
    color: #333;
    selection-background-color: #07C160;
}
QLineEdit:focus {
    border-color: #07C160;
}

/* ── 按钮 ────────────────────────────────────────────────────── */
QPushButton {
    background: #07C160;
    color: #FFFFFF;
    border: none;
    border-radius: 6px;
    padding: 6px 18px;
    font-size: 13px;
}
QPushButton:hover {
    background: #06AD56;
}
QPushButton:pressed {
    background: #059A4C;
}
QPushButton:disabled {
    background: #B0B0B0;
    color: #888;
}

/* 危险操作按钮（objectName = "dangerButton"）*/
QPushButton#dangerButton {
    background: #E53E3E;
}
QPushButton#dangerButton:hover {
    background: #C53030;
}

/* ── 滚动条 ──────────────────────────────────────────────────── */
QScrollBar:vertical {
    width: 6px;
    background: transparent;
}
QScrollBar::handle:vertical {
    background: #C0C0C0;
    border-radius: 3px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover {
    background: #A0A0A0;
}
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
    background: none;
    border: none;
}
QScrollBar:horizontal { height: 0px; }

/* ── 状态栏 ──────────────────────────────────────────────────── */
QStatusBar {
    background: #F0F0F0;
    color: #666;
    font-size: 12px;
    border-top: 1px solid #E0E0E0;
}

/* ── 对话框 ──────────────────────────────────────────────────── */
QDialog {
    background: #FFFFFF;
}

/* ── 菜单 ────────────────────────────────────────────────────── */
QMenuBar {
    background: #FAFAFA;
    border-bottom: 1px solid #E8E8E8;
}
QMenuBar::item:selected {
    background: #E8F8EF;
    color: #07C160;
}
QMenu {
    background: #FFFFFF;
    border: 1px solid #E0E0E0;
    border-radius: 4px;
}
QMenu::item:selected {
    background: #E8F8EF;
    color: #07C160;
}
```

- [ ] **Step 3：写 AppStyle 的 loadStyleSheet() 实现**

在 `client/src/ui/AppStyle.h` 中直接内联实现（避免单独的 .cpp 文件）：

```cpp
// 在 AppStyle.h 末尾，命名空间内追加：
inline QString loadStyleSheet() {
    QFile qss(":/style.qss");
    if (!qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(qss.readAll());
}
```

同时在文件顶部添加：
```cpp
#include <QFile>
#include <QString>
```

- [ ] **Step 4：将 style.qss 加入 CMake 资源系统**

在 `client/CMakeLists.txt` 中添加 Qt 资源文件（若尚未有 resources 机制）：

在 CMakeLists.txt 中追加一个 `.qrc` 文件引用：

创建 `client/resources/resources.qrc`：
```xml
<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="/">
        <file>style.qss</file>
    </qresource>
</RCC>
```

在 `client/CMakeLists.txt` 中，将 `resources.qrc` 加入可执行文件的源列表：
```cmake
add_executable(NovaChat WIN32
    src/main.cpp
    resources/resources.qrc    # ← 追加
)
```

（Qt 的 AUTORCC 会自动处理 `.qrc` 文件）

- [ ] **Step 5：在 main.cpp 应用样式表**

在 `QApplication app(argc, argv);` 之后添加：

```cpp
#include "ui/AppStyle.h"
// ...
app.setStyle("Fusion");   // 统一 Fusion 基础样式，再叠加 QSS
const QString qss = AppStyle::loadStyleSheet();
if (!qss.isEmpty()) {
    app.setStyleSheet(qss);
} else {
    Logger::warn("样式表加载失败，使用默认样式");
}
```

- [ ] **Step 6：编译验证**

```
cmake --build --preset windows-debug
```

预期：编译无报错，启动后界面应用新样式。

- [ ] **Step 7：目视检查**

启动客户端，确认：
- 按钮为绿色圆角
- 列表项有底部分隔线，hover 时变灰色
- 输入框 focus 时有绿色描边
- 滚动条细化为 6px
- 状态栏字体和颜色正确

- [ ] **Step 8：提交**

```
git add client/resources/style.qss client/resources/resources.qrc
git add client/src/ui/AppStyle.h client/src/main.cpp
git add client/CMakeLists.txt
git commit -m "feat(ui): 应用全局 QSS 样式，统一视觉风格"
```

---

## Task 33：集成验证 + 最终收尾

**Files:**
- Create: `docs/superpowers/plans/integration-checklist.md`（本 Task 同时作为验证手册）

> 本 Task 不添加新功能，专注于运行完整场景，修复发现的问题，确保 V1 功能可用。

- [ ] **Step 1：运行全部单元测试，确认 43 个测试 PASS**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：所有 43 个测试 PASS，0 FAIL。

- [ ] **Step 2：场景一 — 完整注册+登录+加好友+聊天流程**

启动服务端：
```
./build/debug/server/NovaChatServer.exe
```

**客户端 A（alice）：**
1. 启动 → 显示 LoginWindow → 点「注册」
2. 填写：用户名 `alice`，昵称 `Alice`，密码 `abc123` → 注册成功 → 自动跳转到 LoginWindow
3. 填写 alice / abc123 → 登录
4. 主窗口标题显示「NovaChat — Alice」，状态栏显示「🟢 已连接」
5. 联系人 Tab → 点「+ 添加好友」→ 搜索 `bob` → 无结果（bob 尚未注册，正常）

**客户端 B（bob）：**
1. 启动 → 注册 `bob` / `Bob` / `abc123` → 登录

**客户端 A 继续：**
6. 重新搜索 `bob` → 找到 → 点「添加为好友」→ 对话框显示「已成功添加好友！」
7. 关闭 AddFriendDialog → 联系人列表显示 Bob
8. 双击 Bob → 右侧打开聊天区（空）
9. 输入「你好 Bob！」→ Enter 发送 → 绿色气泡出现在右侧

**客户端 B：**
10. 会话 Tab 出现与 alice 的会话，显示未读徽标 `[1]`
11. 双击会话 → 右侧显示「你好 Bob！」气泡（灰色，左对齐）
12. 输入「你好 Alice！」→ 发送

**客户端 A：**
13. 实时收到「你好 Alice！」气泡（灰色）
14. 会话 Tab 的 alice↔bob 会话显示正确的最新消息预览

✅ **期望结果：** 所有步骤完成，无崩溃，消息实时收发

- [ ] **Step 3：场景二 — 重启恢复**

1. 关闭两个客户端（服务端保持运行）
2. 重新启动客户端 A
3. 验证：自动登录（无需输入账号密码）
4. 验证：联系人列表显示 Bob
5. 验证：会话 Tab 显示与 Bob 的历史会话
6. 双击会话 → 聊天区显示历史消息（从本地 SQLite 加载）

✅ **期望结果：** 所有历史数据从本地恢复，无需重新联网拉取

- [ ] **Step 4：场景三 — 断线重连**

1. 两个客户端均已登录，正常聊天
2. 关闭服务端进程
3. 观察客户端状态栏：从「🟢 已连接」变为「🔴 断开连接」→「🟡 重连中 (2s)...」
4. 重新启动服务端
5. 客户端自动重连，状态栏恢复「🟢 已连接」，显示「连接已恢复」通知
6. 发送一条新消息，正常收发

✅ **期望结果：** 断线无崩溃，重连后正常工作

- [ ] **Step 5：场景四 — 退出登录 + 切换账号**

1. 客户端 A 点击「文件 → 设置」
2. 对话框显示 alice 的账号信息
3. 点击「退出登录」→ 跳回 LoginWindow
4. 登录 bob 账号
5. 主窗口标题更新为「NovaChat — Bob」

✅ **期望结果：** 退出登录后本地 is_active 清零，重新登录显示正确账号

- [ ] **Step 6：场景五 — 边界情况**

| 测试 | 操作 | 期望结果 |
|------|------|---------|
| 空消息 | 输入框为空时按发送 | 不发送，无报错 |
| 超长消息 | 发送 500 字中文 | 气泡正确换行，不截断 |
| 快速连续发送 | 连续点击发送 5 次 | 5 条消息依次出现，顺序正确 |
| 重复添加好友 | 对已添加的好友再次添加 | 提示「已是好友或用户不存在」 |
| 错误密码登录 | 密码错误登录 | LoginWindow 显示错误提示 |

- [ ] **Step 7：修复所有在集成测试中发现的 Bug**

对每个发现的问题：
1. 定位根因（哪个文件、哪个函数）
2. 修复代码
3. 重新运行 `ctest` 确认不破坏既有测试
4. 提交修复

- [ ] **Step 8：最终全量构建 + 完整测试**

```
cmake --build --preset windows-release
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：
- Debug 和 Release 均编译无报错无警告
- 43 个测试全部 PASS

- [ ] **Step 9：写集成验证结论**

创建 `docs/superpowers/plans/integration-checklist.md`，记录：
- 每个场景的验证结果（✅ / ❌ / ⚠️）
- 发现并修复的 Bug 列表
- V1 已知限制（群聊/图片/音视频等未做）

- [ ] **Step 10：最终提交**

```
git add .
git commit -m "chore: V1 集成验证通过，修复发现的 Bug，完成收尾"
git tag v1.0.0
```

---

## 阶段六完成标准

- [ ] `ctest --preset windows-debug` 43 个测试全部 PASS
- [ ] Debug 和 Release 均编译无警告
- [ ] 断网后 2s 内显示「🔴 断开连接」；服务端重启后自动重连
- [ ] AUTH_EXPIRED 时自动清除 token 并跳回 LoginWindow
- [ ] 所有服务错误信息在状态栏展示，不打断用户操作流
- [ ] SettingsDialog 正确显示账号信息，退出登录按钮可用
- [ ] 全局 QSS 已应用，UI 视觉风格一致
- [ ] 5 个集成场景全部 ✅
- [ ] `git tag v1.0.0` 已打

---

## V1 完整功能清单（各阶段汇总）

| 功能 | 实现阶段 | 状态 |
|------|---------|------|
| CMake 工程骨架 + vcpkg | 阶段一 | ✅ |
| Logger（spdlog）+ 单元测试 | 阶段一 | ✅ |
| AppConfig（yaml-cpp）| 阶段二 | ✅ |
| DatabaseManager（SQLite WAL）| 阶段二 | ✅ |
| Proto 定义 + ProtocolCodec | 阶段二 | ✅ |
| TcpClient + TcpServer + 心跳 | 阶段二 | ✅ |
| 注册 / 登录 / 自动登录 / 退出登录 | 阶段三 | ✅ |
| Token 持久化 + 多账号隔离 | 阶段三 | ✅ |
| 好友列表同步 | 阶段四 | ✅ |
| 添加 / 删除 / 搜索好友 | 阶段四 | ✅ |
| 实时文字消息收发 | 阶段五 | ✅ |
| 历史记录分页加载（50 条/页）| 阶段五 | ✅ |
| 会话列表 + 未读数 | 阶段五 | ✅ |
| 气泡式 ChatView | 阶段五 | ✅ |
| 断线自动重连（指数退避）| 阶段六 | ✅ |
| AUTH_EXPIRED 强制登出 | 阶段六 | ✅ |
| SettingsDialog | 阶段六 | ✅ |
| 统一错误通知 + 连接状态栏 | 阶段六 | ✅ |
| 全局 QSS 样式 | 阶段六 | ✅ |
