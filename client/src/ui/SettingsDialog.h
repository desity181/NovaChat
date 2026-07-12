#pragma once
#include "core/User.h"
#include "service/AuthService.h"
#include <QDialog>

// Account settings dialog.
// V1: displays account info and provides a log-out entry point.
// Future: edit display name, avatar, notification preferences.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const User&  user,
                             AuthService* auth_service,
                             QWidget*     parent = nullptr);

signals:
    // Emitted when the user clicks Log Out; handled by MainWindow.
    void logoutRequested();

private:
    AuthService* auth_service_;
};
