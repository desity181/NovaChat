#include "ui/ContactPanel.h"
#include "ui/AddFriendDialog.h"
#include <QAction>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>
#include <algorithm>

ContactPanel::ContactPanel(ContactService* service, QWidget* parent)
    : QWidget(parent)
    , service_(service)
{
    search_edit_ = new QLineEdit(this);
    add_btn_     = new QPushButton("+ Add", this);
    list_widget_ = new QListWidget(this);
    count_label_ = new QLabel(this);

    search_edit_->setPlaceholderText("Search contacts...");
    list_widget_->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* top_row = new QHBoxLayout;
    top_row->addWidget(search_edit_);
    top_row->addWidget(add_btn_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addLayout(top_row);
    layout->addWidget(list_widget_);
    layout->addWidget(count_label_);

    contacts_ = service_->contacts();
    populateList();

    connect(search_edit_, &QLineEdit::textChanged,
            this, &ContactPanel::onSearchChanged);
    connect(add_btn_, &QPushButton::clicked,
            this, &ContactPanel::onAddFriendClicked);
    connect(list_widget_, &QListWidget::itemDoubleClicked,
            this, &ContactPanel::onItemDoubleClicked);
    connect(list_widget_, &QListWidget::customContextMenuRequested,
            this, &ContactPanel::onContextMenuRequested);
    connect(service_, &ContactService::contactsSynced,
            this, &ContactPanel::onContactsSynced);
    connect(service_, &ContactService::contactAdded,
            this, &ContactPanel::onContactAdded);
    connect(service_, &ContactService::contactDeleted,
            this, &ContactPanel::onContactDeleted);
}

void ContactPanel::populateList(const QString& filter) {
    list_widget_->clear();
    for (const auto& c : contacts_) {
        const QString display = QString::fromStdString(c.display_name);
        const QString name    = QString::fromStdString(c.username);
        if (!filter.isEmpty() &&
            !display.contains(filter, Qt::CaseInsensitive) &&
            !name.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        auto* item = new QListWidgetItem(
            QString("%1\n%2").arg(display, name), list_widget_);
        item->setData(Qt::UserRole, QString::fromStdString(c.user_id));
    }
    count_label_->setText(
        QString("%1 contact(s)").arg(static_cast<int>(contacts_.size())));
}

void ContactPanel::onSearchChanged(const QString& text) {
    populateList(text.trimmed());
}

void ContactPanel::onAddFriendClicked() {
    auto* dlg = new AddFriendDialog(service_, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

void ContactPanel::onContactsSynced(const std::vector<Contact>& contacts) {
    contacts_ = contacts;
    populateList(search_edit_->text().trimmed());
}

void ContactPanel::onContactAdded(const Contact& contact) {
    contacts_.push_back(contact);
    populateList(search_edit_->text().trimmed());
}

void ContactPanel::onContactDeleted(const std::string& user_id) {
    contacts_.erase(
        std::remove_if(contacts_.begin(), contacts_.end(),
            [&user_id](const Contact& c) { return c.user_id == user_id; }),
        contacts_.end());
    populateList(search_edit_->text().trimmed());
}

void ContactPanel::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    const QString uid = item->data(Qt::UserRole).toString();
    for (const auto& c : contacts_) {
        if (c.user_id == uid.toStdString()) {
            emit contactSelected(c);
            return;
        }
    }
}

void ContactPanel::onContextMenuRequested(const QPoint& pos) {
    auto* item = list_widget_->itemAt(pos);
    if (!item) return;

    const QString uid = item->data(Qt::UserRole).toString();
    QMenu menu(this);
    auto* del_act = menu.addAction("Remove friend");

    connect(del_act, &QAction::triggered, this, [this, uid, item] {
        const QString name = item->text().section('\n', 0, 0);
        auto btn = QMessageBox::question(
            this, "Confirm",
            QString("Remove %1 from your contacts?").arg(name));
        if (btn == QMessageBox::Yes) {
            service_->deleteFriend(uid.toStdString());
        }
    });

    menu.exec(list_widget_->mapToGlobal(pos));
}
