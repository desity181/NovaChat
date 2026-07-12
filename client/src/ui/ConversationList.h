#pragma once
#include "core/Conversation.h"
#include "core/Message.h"
#include "service/ChatService.h"
#include <QListWidget>
#include <QWidget>
#include <vector>

// Recent conversations list.
// · Ordered by last_message_time descending.
// · Displays unread count badge.
// · Double-click emits conversationSelected.
class ConversationList : public QWidget {
    Q_OBJECT
public:
    explicit ConversationList(ChatService* service,
                               QWidget*     parent = nullptr);

signals:
    void conversationSelected(std::string conversation_id,
                               std::string peer_user_id);

private slots:
    void onConversationsLoaded(const std::vector<Conversation>& convs);
    void onMessageReceived    (const Message& msg);
    void onItemDoubleClicked  (QListWidgetItem* item);

private:
    void refresh(const std::vector<Conversation>& convs);

    ChatService* service_;
    QListWidget* list_;
    std::vector<Conversation> conversations_;
};
