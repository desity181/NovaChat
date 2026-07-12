#pragma once
#include "core/Contact.h"
#include "core/User.h"
#include "network/TcpClient.h"
#include "storage/ContactRepository.h"
#include <QObject>
#include <QString>
#include <vector>

// Contact business service.
// · syncContacts(): fetches the full friend list from the server and persists it.
// · addFriend() / deleteFriend(): mutate server state and update the local cache.
// · searchUser(): server-side username search.
// All public methods and signals run on the main (Qt) thread.
class ContactService : public QObject {
    Q_OBJECT
public:
    ContactService(TcpClient*         client,
                   ContactRepository& repo,
                   const User&        current_user,
                   QObject*           parent = nullptr);

    void syncContacts();
    void addFriend   (const std::string& target_user_id);
    void deleteFriend(const std::string& target_user_id);
    void searchUser  (const std::string& keyword);

    const std::vector<Contact>& contacts() const { return contacts_; }

signals:
    void contactsSynced (std::vector<Contact> contacts);
    void contactAdded   (Contact contact);
    void contactDeleted (std::string user_id);
    void searchResult   (std::vector<Contact> users);
    void operationFailed(const QString& reason);
    void authExpired    ();

private:
    TcpClient*         client_;
    ContactRepository& repo_;
    User               current_user_;  // Owned copy — never a dangling reference
    std::vector<Contact> contacts_;  // In-memory mirror of the DB cache
};
