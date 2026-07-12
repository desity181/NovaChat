#pragma once
#include "core/Contact.h"
#include "service/ContactService.h"
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>
#include <vector>

// Contact list panel.
// · Local filter search (hides/shows list items; no server call).
// · "Add friend" opens AddFriendDialog.
// · Right-click context menu to delete a friend.
class ContactPanel : public QWidget {
    Q_OBJECT
public:
    explicit ContactPanel(ContactService* service,
                           QWidget*        parent = nullptr);

signals:
    // Emitted when the user double-clicks a contact (for opening a chat).
    void contactSelected(Contact contact);

private slots:
    void onSearchChanged(const QString& text);
    void onAddFriendClicked();
    void onContactsSynced(const std::vector<Contact>& contacts);
    void onContactAdded  (const Contact& contact);
    void onContactDeleted(const std::string& user_id);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onContextMenuRequested(const QPoint& pos);

private:
    void populateList(const QString& filter = {});

    ContactService*      service_;
    std::vector<Contact> contacts_;  // Mirror of ContactService cache

    QLineEdit*   search_edit_;
    QPushButton* add_btn_;
    QListWidget* list_widget_;
    QLabel*      count_label_;
};
