#pragma once
#include "service/AuthService.h"
#include "core/User.h"
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

// Login window shown when no active local account exists.
// Emits loginSucceeded when the user authenticates successfully.
class LoginWindow : public QWidget {
    Q_OBJECT
public:
    explicit LoginWindow(AuthService* auth_service,
                          QWidget*     parent = nullptr);

signals:
    void loginSucceeded(User user);

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onLoginFailed(const QString& reason);

private:
    AuthService* auth_service_;

    QLineEdit*   username_edit_;
    QLineEdit*   password_edit_;
    QPushButton* login_btn_;
    QPushButton* register_btn_;
    QLabel*      error_label_;
};
