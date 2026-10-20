#pragma once

#include <QDialog>

// Blocking sign-in dialog that gates the main window. One dialog toggling
// Login <-> Register in place (no second window). Passwords are plaintext
// demo-grade (owner ruling); validation is ONLY non-empty password +
// unique (case-sensitive exact match) username.
class SignInDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SignInDialog(QWidget* parent = nullptr);

    // The username the user logged in with (moves into MainWindow).
    QString user() const { return user_; }

signals:
    void acceptedWithUser(const QString& username);

private slots:
    void onRegisterToggle();
    void onLogin();
    void onRegister();

private:
    void showLoginMode();

    class QLineEdit* usernameField = nullptr;
    class QLineEdit* passwordField = nullptr;
    class QPushButton* loginButton = nullptr;
    class QPushButton* registerButton = nullptr;
    class QPushButton* registerToggle = nullptr;
    class QLineEdit* signinStatus = nullptr;
    class QLineEdit* registerStatus = nullptr;
    bool registerMode = false;
    QString user_;
};
