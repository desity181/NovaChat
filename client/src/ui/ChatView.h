#pragma once
#include "core/Message.h"
#include "service/ChatService.h"
#include "ui/MessageBubbleDelegate.h"
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QStandardItemModel>
#include <QString>
#include <QWidget>
#include <vector>

// Single-conversation chat area.
// Top: QListView with bubble delegate (supports scroll-up pagination).
// Bottom: text input + send button (Enter key also sends).
class ChatView : public QWidget {
    Q_OBJECT
public:
    explicit ChatView(ChatService*   service,
                      const QString& own_user_id,
                      QWidget*       parent = nullptr);

    // Switch to a conversation: clears current content and loads history.
    void openConversation(const std::string& conversation_id,
                          const std::string& peer_user_id);

signals:
    void messageSent(const std::string& conversation_id);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onSendClicked();
    void onHistoryLoaded(const std::string& conv_id,
                         const std::vector<Message>& messages,
                         bool has_more);
    void onMessageReceived(const Message& msg);
    void onMessageSent    (const Message& msg);

private:
    void appendMessage(const Message& msg);
    void scrollToBottom();

    ChatService*           service_;
    QString                own_user_id_;
    std::string            current_conv_id_;

    QListView*             message_list_;
    QStandardItemModel*    model_;
    MessageBubbleDelegate* delegate_;
    QLineEdit*             input_edit_;
    QPushButton*           send_btn_;

    bool has_more_{false};
    bool loading_history_{false};
};
