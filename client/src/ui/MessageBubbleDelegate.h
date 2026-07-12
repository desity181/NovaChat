#pragma once
#include <QStyledItemDelegate>

// Custom message bubble renderer for QListView.
//
// Item data roles:
//   UserRole+1  content      (QString)
//   UserRole+2  sender_id    (QString)
//   UserRole+3  timestamp_ms (qlonglong)
//   UserRole+4  is_outgoing  (bool)
//   UserRole+5  status       (int: 0=Sending 1=Sent 2=Failed)
class MessageBubbleDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit MessageBubbleDelegate(const QString& own_user_id,
                                    QObject* parent = nullptr);

    void  paint    (QPainter*                   painter,
                    const QStyleOptionViewItem& option,
                    const QModelIndex&          index) const override;

    QSize sizeHint (const QStyleOptionViewItem& option,
                    const QModelIndex&          index) const override;

private:
    static constexpr int kBubbleMaxWidthPct = 65;
    static constexpr int kPadH       = 12;
    static constexpr int kPadV       = 8;
    static constexpr int kRadius     = 10;
    static constexpr int kItemVMargin = 6;

    QString own_user_id_;
};
