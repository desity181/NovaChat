#pragma once
#include "core/Contact.h"
#include "service/ContactService.h"
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <vector>

// Dialog for searching and adding friends.
class AddFriendDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddFriendDialog(ContactService* service,
                              QWidget*        parent = nullptr);

private slots:
    void onSearchClicked();
    void onAddClicked();
    void onSearchResult(const std::vector<Contact>& users);
    void onOperationFailed(const QString& reason);

private:
    ContactService*      service_;
    std::vector<Contact> results_;  // Current search results

    QLineEdit*   search_edit_;
    QPushButton* search_btn_;
    QListWidget* result_list_;
    QPushButton* add_btn_;
    QLabel*      status_label_;
};
