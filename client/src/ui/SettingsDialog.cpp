#include "ui/SettingsDialog.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const User&  user,
                                AuthService* auth_service,
                                QWidget*     parent)
    : QDialog(parent)
    , auth_service_(auth_service)
{
    setWindowTitle("Account Settings");
    setMinimumWidth(380);
    setModal(true);

    // ── Read-only account info ──────────────────────────────────
    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);

    auto makeField = [](const std::string& value) -> QLabel* {
        auto* lbl = new QLabel(QString::fromStdString(value));
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return lbl;
    };

    form->addRow("User ID:",      makeField(user.server_id));
    form->addRow("Username:",     makeField(user.username));
    form->addRow("Display name:", makeField(user.display_name));
    if (!user.avatar_url.empty()) {
        form->addRow("Avatar URL:", makeField(user.avatar_url));
    }

    // ── Buttons ──────────────────────────────────────────────────
    auto* logout_btn = new QPushButton("Log Out", this);
    auto* close_btn  = new QPushButton("Close",   this);
    logout_btn->setObjectName("dangerButton");

    auto* btn_row = new QHBoxLayout;
    btn_row->addWidget(logout_btn);
    btn_row->addStretch();
    btn_row->addWidget(close_btn);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);
    layout->addLayout(form);
    layout->addSpacing(8);
    layout->addLayout(btn_row);

    connect(close_btn,  &QPushButton::clicked, this, &QDialog::accept);
    connect(logout_btn, &QPushButton::clicked, this, [this] {
        accept();
        emit logoutRequested();
    });
}
