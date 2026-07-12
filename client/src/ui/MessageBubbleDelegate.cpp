#include "ui/MessageBubbleDelegate.h"
#include <QDateTime>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace BubbleRole {
    constexpr int Content    = Qt::UserRole + 1;
    constexpr int SenderId   = Qt::UserRole + 2;
    constexpr int Timestamp  = Qt::UserRole + 3;
    constexpr int IsOutgoing = Qt::UserRole + 4;
    constexpr int Status     = Qt::UserRole + 5;
}

MessageBubbleDelegate::MessageBubbleDelegate(const QString& own_user_id,
                                               QObject* parent)
    : QStyledItemDelegate(parent)
    , own_user_id_(own_user_id)
{}

void MessageBubbleDelegate::paint(QPainter*                   painter,
                                   const QStyleOptionViewItem& option,
                                   const QModelIndex&          index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const bool    is_out = index.data(BubbleRole::IsOutgoing).toBool();
    const QString text   = index.data(BubbleRole::Content).toString();
    const int64_t ts_ms  = index.data(BubbleRole::Timestamp).toLongLong();
    const int     status = index.data(BubbleRole::Status).toInt();

    const QString ts_str = QDateTime::fromMSecsSinceEpoch(ts_ms).toString("HH:mm");
    const QString foot   = is_out
        ? (status == 2 ? "Failed " + ts_str : ts_str)
        : ts_str;

    const QRect total_rect = option.rect.adjusted(8, kItemVMargin, -8, -kItemVMargin);
    const int max_bubble_w = total_rect.width() * kBubbleMaxWidthPct / 100;

    QFont font = option.font;
    QFontMetrics fm(font);
    QFont small_font = font;
    small_font.setPointSize(std::max(font.pointSize() - 2, 7));
    QFontMetrics small_fm(small_font);

    const int text_max_w = max_bubble_w - 2 * kPadH;
    const QRect text_bounding = fm.boundingRect(
        QRect(0, 0, text_max_w, 9999), Qt::TextWordWrap, text);
    const int bubble_w = std::min(text_bounding.width() + 2 * kPadH, max_bubble_w);
    const int bubble_h = text_bounding.height() + 2 * kPadV;

    const int bubble_x = is_out ? total_rect.right() - bubble_w : total_rect.left();
    const QRect bubble_rect(bubble_x, total_rect.top(), bubble_w, bubble_h);

    const QColor bubble_color = is_out ? QColor(0x07, 0xC1, 0x60)
                                        : QColor(0xF0, 0xF0, 0xF0);
    painter->setPen(Qt::NoPen);
    painter->setBrush(bubble_color);
    painter->drawRoundedRect(bubble_rect, kRadius, kRadius);

    painter->setPen(is_out ? Qt::white : Qt::black);
    painter->setFont(font);
    painter->drawText(bubble_rect.adjusted(kPadH, kPadV, -kPadH, -kPadV),
                      Qt::TextWordWrap, text);

    painter->setFont(small_font);
    painter->setPen(QColor(0xAA, 0xAA, 0xAA));
    const int foot_y = bubble_rect.bottom() + 2;
    const int align  = is_out ? Qt::AlignRight : Qt::AlignLeft;
    painter->drawText(QRect(total_rect.left(), foot_y, total_rect.width(), 20),
                      align | Qt::AlignTop, foot);

    painter->restore();
}

QSize MessageBubbleDelegate::sizeHint(const QStyleOptionViewItem& option,
                                       const QModelIndex&          index) const {
    const QString text  = index.data(BubbleRole::Content).toString();
    const int view_w    = option.rect.isValid() ? option.rect.width() : 500;
    const int max_w     = view_w * kBubbleMaxWidthPct / 100;
    const int text_max_w = max_w - 2 * kPadH;

    QFontMetrics fm(option.font);
    const QRect text_bounding = fm.boundingRect(
        QRect(0, 0, text_max_w, 9999), Qt::TextWordWrap, text);

    const int h = kItemVMargin + kPadV + text_bounding.height() + kPadV
                + 20 + kItemVMargin;
    return {view_w, h};
}
