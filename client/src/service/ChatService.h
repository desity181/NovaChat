#pragma once
#include "core/Conversation.h"
#include "core/Message.h"
#include "core/User.h"
#include "network/TcpClient.h"
#include "storage/ConversationRepository.h"
#include "storage/MessageRepository.h"
#include <QObject>
#include <QString>
#include <vector>

// Chat business service.
// · sendMessage(): optimistic local write then server confirm.
// · loadHistory(): server-side paged retrieval, persisted locally.
// · syncConversations(): pull conversation list from server on login.
// · onPacketReceived(): handles server-pushed MessagePush packets.
// All public methods and signals run on the main (Qt) thread.
class ChatService : public QObject {
    Q_OBJECT
public:
    ChatService(TcpClient*              client,
                MessageRepository&      msg_repo,
                ConversationRepository& conv_repo,
                const User&             current_user,
                QObject*                parent = nullptr);

    void sendMessage(const std::string& receiver_id,
                     const std::string& content);

    void loadHistory(const std::string& conversation_id,
                     int64_t            before_timestamp = 0);

    void syncConversations();

    // Clear the unread counter for a conversation (called when it is opened).
    void clearUnread(const std::string& conversation_id);

    const std::vector<Conversation>& conversations() const { return conversations_; }

    // Deterministic conversation ID: min(a,b) + ":" + max(a,b)
    static std::string makeConvId(const std::string& a, const std::string& b);

signals:
    void messageSent       (Message message);
    void messageFailed     (std::string local_id);
    void messageReceived   (Message message);
    void historyLoaded     (std::string conv_id,
                            std::vector<Message> messages,
                            bool has_more);
    void conversationsLoaded(std::vector<Conversation> conversations);
    void authExpired        ();

private slots:
    void onPacketReceived(Packet pkt);

private:
    void handleMessagePush(const Packet& pkt);

    TcpClient*              client_;
    MessageRepository&      msg_repo_;
    ConversationRepository& conv_repo_;
    User                    current_user_;  // Owned copy — never a dangling reference
    std::vector<Conversation> conversations_;

    uint64_t local_id_counter_{0};
};
