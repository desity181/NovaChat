#pragma once
#include "core/User.h"
#include "network/TcpClient.h"
#include "storage/UserRepository.h"
#include <QObject>
#include <QString>

// Login business service: coordinates TcpClient (network) and UserRepository (persistence).
// All public methods and emitted signals run on the main (Qt) thread.
class AuthService : public QObject {
    Q_OBJECT
public:
    explicit AuthService(TcpClient*      client,
                         UserRepository& user_repo,
                         QObject*        parent = nullptr);

    void login(const std::string& username, const std::string& password);

    void registerUser(const std::string& username,
                      const std::string& password,
                      const std::string& display_name);

    // Send a logout request to the server and clear the local active account.
    void logout();

    const User& currentUser() const { return current_user_; }

signals:
    void loginSucceeded(User user);
    void loginFailed   (const QString& reason);
    void registerSucceeded();
    void registerFailed(const QString& reason);
    void loggedOut();

private:
    TcpClient*      client_;
    UserRepository& user_repo_;
    User            current_user_;
};
