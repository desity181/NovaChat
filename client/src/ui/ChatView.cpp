#include "ui/ChatView.h"
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScrollBar>
#include <QStandardItem>
#include <QVBoxLayout>

namespace BubbleRole {
    constexpr int Content    = Qt::UserRole + 1;
    constexpr int SenderId   = Qt::UserRole + 2;
    constexpr int Timestamp  = Qt::UserRole + 3;
    constexpr int IsOutgoing = Qt::UserRole + 4;
    constexpr int Status     = Qt::UserRole + 5;
}

ChatView::ChatView(ChatService*   service,
                    const QString& own_user_id,
                    QWidget*       parent)
    : QWidget(parent)
    , service_(service)
    , own_user_id_(own_user_id)
{
    model_    = new QStandardItemModel(this);
    delegate_ = new MessageBubbleDelegate(own_user_id_, this);

    message_list_ = new QListView(this);
    message_list_->setModel(model_);
    message_list_->setItemDelegate(delegate_);
    message_list_->setSpacing(2);
    message_list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    message_list_->setSelectionMode(QAbstractItemView::NoSelection);
    message_list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    message_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    input_edit_ = new QLineEdit(this);
    send_btn_   = new QPushButton("Send", this);
    input_edit_->setPlaceholderText("Type a message, Enter to send...");
    input_edit_->installEventFilter(this);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(input_edit_);
    bottom->addWidget(send_btn_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(message_list_);
    layout->addLayout(bottom);

    connect(send_btn_, &QPushButton::clicked, this, &ChatView::onSendClicked);
    connect(service_, &ChatService::historyLoaded,  this, &ChatView::onHistoryLoaded);
    connect(service_, &ChatService::messageReceived, this, &ChatView::onMessageReceived);
    connect(service_, &ChatService::messageSent,     this, &ChatView::onMessageSent);

    // Load more history when scrolled to top
    connect(message_list_->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int value) {
                if (value == 0 && has_more_ && !loading_history_
                    && model_->rowCount() > 0) {
                    loading_history_ = true;
                    const int64_t earliest = model_->item(0)
                        ->data(BubbleRole::Timestamp).toLongLong();
                    service_->loadHistory(current_conv_id_, earliest);
                }
            });
}

bool ChatView::eventFilter(QObject* obj, QEvent* event) {
    if (obj == input_edit_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return
            && !(ke->modifiers() & Qt::ShiftModifier)) {
            onSendClicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ChatView::openConversation(const std::string& conv_id,
                                 const std::string& /*peer_user_id*/) {
    current_conv_id_ = conv_id;
    model_->clear();
    has_more_        = false;
    loading_history_ = false;
    service_->loadHistory(conv_id, 0);
}

void ChatView::onSendClicked() {
    if (current_conv_id_.empty()) return;
    const QString text = input_edit_->text().trimmed();
    if (text.isEmpty()) return;
    input_edit_->clear();

    // Derive receiver from conversation ID (format: min:max)
    const auto colon = current_conv_id_.find(':');
    const std::string id_a = current_conv_id_.substr(0, colon);
    const std::string id_b = current_conv_id_.substr(colon + 1);
    const std::string receiver =
        (id_a == own_user_id_.toStdString()) ? id_b : id_a;

    service_->sendMessage(receiver, text.toStdString());
}

void ChatView::onHistoryLoaded(const std::string& conv_id,
                                const std::vector<Message>& messages,
                                bool has_more) {
    if (conv_id != current_conv_id_) return;

    loading_history_ = false;
    has_more_        = has_more;
    if (messages.empty()) return;

    auto* sb = message_list_->verticalScrollBar();
    const int old_max = sb->maximum();

    for (int i = 0; i < static_cast<int>(messages.size()); ++i) {
        auto* item        = new QStandardItem;
        const Message& m  = messages[i];
        item->setData(QString::fromStdString(m.content),         BubbleRole::Content);
        item->setData(QString::fromStdString(m.sender_id),       BubbleRole::SenderId);
        item->setData(static_cast<qlonglong>(m.timestamp_ms),    BubbleRole::Timestamp);
        item->setData(m.sender_id == own_user_id_.toStdString(), BubbleRole::IsOutgoing);
        item->setData(static_cast<int>(m.status),                BubbleRole::Status);
        model_->insertRow(i, item);
    }

    if (old_max == 0) {
        scrollToBottom();
    } else {
        const int new_max = sb->maximum();
        sb->setValue(sb->value() + (new_max - old_max));
    }
}

void ChatView::onMessageReceived(const Message& msg) {
    if (msg.conversation_id != current_conv_id_) return;
    appendMessage(msg);
}

void ChatView::onMessageSent(const Message& msg) {
    if (msg.conversation_id != current_conv_id_) return;
    appendMessage(msg);
}

void ChatView::appendMessage(const Message& msg) {
    auto* item = new QStandardItem;
    item->setData(QString::fromStdString(msg.content),          BubbleRole::Content);
    item->setData(QString::fromStdString(msg.sender_id),        BubbleRole::SenderId);
    item->setData(static_cast<qlonglong>(msg.timestamp_ms),     BubbleRole::Timestamp);
    item->setData(msg.sender_id == own_user_id_.toStdString(),  BubbleRole::IsOutgoing);
    item->setData(static_cast<int>(msg.status),                 BubbleRole::Status);
    model_->appendRow(item);
    scrollToBottom();
}

void ChatView::scrollToBottom() {
    QMetaObject::invokeMethod(this, [this] {
        auto* sb = message_list_->verticalScrollBar();
        sb->setValue(sb->maximum());
    }, Qt::QueuedConnection);
}
