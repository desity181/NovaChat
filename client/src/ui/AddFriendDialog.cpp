#include "ui/AddFriendDialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>

AddFriendDialog::AddFriendDialog(ContactService* service, QWidget* parent)
    : QDialog(parent)
    , service_(service)
{
    setWindowTitle("Add Friend");
    setMinimumWidth(360);

    search_edit_  = new QLineEdit(this);
    search_btn_   = new QPushButton("Search", this);
    result_list_  = new QListWidget(this);
    add_btn_      = new QPushButton("Add as friend", this);
    status_label_ = new QLabel(this);

    search_edit_->setPlaceholderText("Enter username to search...");
    add_btn_->setEnabled(false);
    status_label_->setStyleSheet("color: #666;");

    auto* search_row = new QHBoxLayout;
    search_row->addWidget(search_edit_);
    search_row->addWidget(search_btn_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(search_row);
    layout->addWidget(result_list_);
    layout->addWidget(status_label_);
    layout->addWidget(add_btn_);

    connect(search_btn_,  &QPushButton::clicked,
            this, &AddFriendDialog::onSearchClicked);
    connect(search_edit_, &QLineEdit::returnPressed,
            this, &AddFriendDialog::onSearchClicked);
    connect(add_btn_,     &QPushButton::clicked,
            this, &AddFriendDialog::onAddClicked);
    connect(result_list_, &QListWidget::currentRowChanged,
            this, [this](int row) {
                add_btn_->setEnabled(row >= 0);
            });

    connect(service_, &ContactService::searchResult,
            this, &AddFriendDialog::onSearchResult);
    connect(service_, &ContactService::operationFailed,
            this, &AddFriendDialog::onOperationFailed);
    connect(service_, &ContactService::contactAdded,
            this, [this](const Contact&) {
                status_label_->setText("Friend added successfully!");
                add_btn_->setEnabled(false);
            });
}

void AddFriendDialog::onSearchClicked() {
    const QString kw = search_edit_->text().trimmed();
    if (kw.isEmpty()) return;
    result_list_->clear();
    results_.clear();
    add_btn_->setEnabled(false);
    status_label_->setText("Searching...");
    service_->searchUser(kw.toStdString());
}

void AddFriendDialog::onSearchResult(const std::vector<Contact>& users) {
    result_list_->clear();
    results_ = users;
    if (users.empty()) {
        status_label_->setText("No matching users found");
        return;
    }
    status_label_->setText(
        QString("Found %1 user(s)").arg(static_cast<int>(users.size())));
    for (const auto& u : users) {
        result_list_->addItem(
            QString("%1  (%2)")
            .arg(QString::fromStdString(u.display_name))
            .arg(QString::fromStdString(u.username)));
    }
}

void AddFriendDialog::onAddClicked() {
    int row = result_list_->currentRow();
    if (row < 0 || row >= static_cast<int>(results_.size())) return;
    add_btn_->setEnabled(false);
    status_label_->setText("Adding...");
    service_->addFriend(results_[row].user_id);
}

void AddFriendDialog::onOperationFailed(const QString& reason) {
    status_label_->setText(reason);
    add_btn_->setEnabled(true);
}
