#include "ui/LoginWindow.h"
#include "ui/RegisterDialog.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

LoginWindow::LoginWindow(AuthService* auth_service, QWidget* parent)
    : QWidget(parent)
    , auth_service_(auth_service)
{
    setWindowTitle("NovaChat — Login");
    setFixedSize(380, 280);

    username_edit_ = new QLineEdit(this);
    password_edit_ = new QLineEdit(this);
    login_btn_     = new QPushButton("Login", this);
    register_btn_  = new QPushButton("Create account", this);
    error_label_   = new QLabel(this);

    password_edit_->setEchoMode(QLineEdit::Password);
    error_label_->setStyleSheet("color: red;");
    error_label_->hide();

    auto* title = new QLabel("<h2>NovaChat</h2>", this);
    title->setAlignment(Qt::AlignCenter);

    auto* form = new QFormLayout;
    form->addRow("Username:", username_edit_);
    form->addRow("Password:", password_edit_);

    auto* btn_row = new QHBoxLayout;
    btn_row->addWidget(login_btn_);
    btn_row->addWidget(register_btn_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 24, 40, 24);
    layout->setSpacing(12);
    layout->addWidget(title);
    layout->addLayout(form);
    layout->addWidget(error_label_);
    layout->addLayout(btn_row);

    connect(login_btn_,    &QPushButton::clicked,
            this, &LoginWindow::onLoginClicked);
    connect(register_btn_, &QPushButton::clicked,
            this, &LoginWindow::onRegisterClicked);
    connect(auth_service_, &AuthService::loginSucceeded,
            this, [this](User user) {
                emit loginSucceeded(user);
                close();
            });
    connect(auth_service_, &AuthService::loginFailed,
            this, &LoginWindow::onLoginFailed);
}

void LoginWindow::onLoginClicked() {
    const QString username = username_edit_->text().trimmed();
    const QString password = password_edit_->text();

    if (username.isEmpty() || password.isEmpty()) {
        error_label_->setText("Please enter username and password");
        error_label_->show();
        return;
    }

    login_btn_->setEnabled(false);
    error_label_->hide();
    auth_service_->login(username.toStdString(), password.toStdString());
}

void LoginWindow::onRegisterClicked() {
    auto* dlg = new RegisterDialog(auth_service_, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

void LoginWindow::onLoginFailed(const QString& reason) {
    error_label_->setText(reason);
    error_label_->show();
    login_btn_->setEnabled(true);
}
