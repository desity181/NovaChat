#include "ui/ConversationList.h"
#include <QDateTime>
#include <QVBoxLayout>

ConversationList::ConversationList(ChatService* service, QWidget* parent)
    : QWidget(parent)
    , service_(service)
{
    list_ = new QListWidget(this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(list_);

    conversations_ = service_->conversations();
    refresh(conversations_);

    // Both emitter (ChatService) and receiver (ConversationList) live on the main
    // thread, so AutoConnection resolves to DirectConnection — no meta-type
    // registration required for std::vector<Conversation> or Message.
    connect(service_, &ChatService::conversationsLoaded,
            this,     &ConversationList::onConversationsLoaded);
    connect(service_, &ChatService::messageReceived,
            this,     &ConversationList::onMessageReceived);
    connect(list_, &QListWidget::itemDoubleClicked,
            this,  &ConversationList::onItemDoubleClicked);
}

void ConversationList::refresh(const std::vector<Conversation>& convs) {
    list_->clear();
    conversations_ = convs;
    for (const auto& c : convs) {
        const QString ts = QDateTime::fromMSecsSinceEpoch(c.last_message_time)
                               .toString("MM/dd HH:mm");
        QString display = QString("[%1]  %2\n%3")
            .arg(QString::fromStdString(c.target_id))
            .arg(ts)
            .arg(QString::fromStdString(c.last_message_preview));
        if (c.unread_count > 0) {
            display += QString("  [%1]").arg(c.unread_count);
        }
        auto* item = new QListWidgetItem(display, list_);
        item->setData(Qt::UserRole,     QString::fromStdString(c.conversation_id));
        item->setData(Qt::UserRole + 1, QString::fromStdString(c.target_id));
    }
}

void ConversationList::onConversationsLoaded(const std::vector<Conversation>& convs) {
    refresh(convs);
}

void ConversationList::onMessageReceived(const Message& /*msg*/) {
    refresh(service_->conversations());
}

void ConversationList::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    const std::string conv_id = item->data(Qt::UserRole).toString().toStdString();
    const std::string peer_id = item->data(Qt::UserRole + 1).toString().toStdString();
    emit conversationSelected(conv_id, peer_id);
}
