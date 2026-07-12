#include "ui/RegisterDialog.h"
#include <QFormLayout>
#include <QMessageBox>
#include <QVBoxLayout>

RegisterDialog::RegisterDialog(AuthService* auth_service, QWidget* parent)
    : QDialog(parent)
    , auth_service_(auth_service)
{
    setWindowTitle("Register");
    setFixedWidth(360);

    username_edit_     = new QLineEdit(this);
    display_name_edit_ = new QLineEdit(this);
    password_edit_     = new QLineEdit(this);
    confirm_edit_      = new QLineEdit(this);
    register_btn_      = new QPushButton("Register", this);
    error_label_       = new QLabel(this);

    password_edit_->setEchoMode(QLineEdit::Password);
    confirm_edit_->setEchoMode(QLineEdit::Password);
    error_label_->setStyleSheet("color: red;");
    error_label_->hide();

    auto* form = new QFormLayout;
    form->addRow("Username:",         username_edit_);
    form->addRow("Display name:",     display_name_edit_);
    form->addRow("Password:",         password_edit_);
    form->addRow("Confirm password:", confirm_edit_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(error_label_);
    layout->addWidget(register_btn_);

    connect(register_btn_,  &QPushButton::clicked,
            this, &RegisterDialog::onRegisterClicked);
    connect(auth_service_, &AuthService::registerSucceeded,
            this, &RegisterDialog::onRegisterSucceeded);
    connect(auth_service_, &AuthService::registerFailed,
            this, &RegisterDialog::onRegisterFailed);
}

void RegisterDialog::onRegisterClicked() {
    const QString username = username_edit_->text().trimmed();
    const QString password = password_edit_->text();
    const QString confirm  = confirm_edit_->text();
    const QString dispName = display_name_edit_->text().trimmed();

    if (username.isEmpty() || password.isEmpty()) {
        error_label_->setText("Username and password are required");
        error_label_->show();
        return;
    }
    if (password != confirm) {
        error_label_->setText("Passwords do not match");
        error_label_->show();
        return;
    }

    register_btn_->setEnabled(false);
    error_label_->hide();
    auth_service_->registerUser(username.toStdString(),
                                 password.toStdString(),
                                 dispName.toStdString());
}

void RegisterDialog::onRegisterSucceeded() {
    QMessageBox::information(this, "Success",
                             "Account created. Please log in with your new credentials.");
    accept();
}

void RegisterDialog::onRegisterFailed(const QString& reason) {
    error_label_->setText(reason);
    error_label_->show();
    register_btn_->setEnabled(true);
}
