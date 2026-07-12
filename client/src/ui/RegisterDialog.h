#pragma once
#include "service/AuthService.h"
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

// Registration dialog opened from LoginWindow.
class RegisterDialog : public QDialog {
    Q_OBJECT
public:
    explicit RegisterDialog(AuthService* auth_service,
                             QWidget*     parent = nullptr);

private slots:
    void onRegisterClicked();
    void onRegisterSucceeded();
    void onRegisterFailed(const QString& reason);

private:
    AuthService* auth_service_;

    QLineEdit*   username_edit_;
    QLineEdit*   display_name_edit_;
    QLineEdit*   password_edit_;
    QLineEdit*   confirm_edit_;
    QPushButton* register_btn_;
    QLabel*      error_label_;
};
