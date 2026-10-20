#include "signin_dialog.h"

#include <QHash>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "db.h"

namespace {

// Sets the stable identifier on BOTH objectName and accessibleName
// (same contract as mainwindow.cpp; see plan Global Constraints).
QWidget* makeStable(QWidget* widget, const char* id)
{
    widget->setObjectName(QString::fromLatin1(id));
    widget->setAccessibleName(QString::fromLatin1(id));
    return widget;
}

// Pinned copy (plan Task 3): these strings are part of the automation
// contract and must not drift.
constexpr char kWrongPassword[] = "Wrong password.";
constexpr char kUnknownUsername[] = "Unknown username.";
constexpr char kUsernameExists[] = "Username already exists.";
constexpr char kPasswordEmpty[] = "Password can not be empty.";
constexpr char kUsernameEmpty[] = "Username can not be empty.";
constexpr char kRegistered[] = "Registered \xE2\x80\x94 log in."; // "Registered — log in."

} // namespace

SignInDialog::SignInDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sign in - Iran Signal Tracker"));
    makeStable(this, "signinWindow");

    auto* userLabel = new QLabel(tr("Username"), this);
    auto* userField = new QLineEdit(this);
    usernameField = qobject_cast<QLineEdit*>(makeStable(userField, "usernameField"));

    auto* passLabel = new QLabel(tr("Password"), this);
    auto* passField = new QLineEdit(this);
    passField->setEchoMode(QLineEdit::Password);
    passwordField = qobject_cast<QLineEdit*>(makeStable(passField, "passwordField"));

    auto* loginBtn = new QPushButton(tr("Log in"), this);
    loginButton = qobject_cast<QPushButton*>(makeStable(loginBtn, "loginButton"));
    auto* registerBtn = new QPushButton(tr("Register"), this);
    registerButton = qobject_cast<QPushButton*>(makeStable(registerBtn, "registerButton"));
    auto* toggleBtn = new QPushButton(tr("Create account"), this);
    registerToggle = qobject_cast<QPushButton*>(makeStable(toggleBtn, "registerToggle"));

    signinStatus = qobject_cast<QLabel*>(makeStable(new QLabel(this), "signinStatus"));
    registerStatus = qobject_cast<QLabel*>(makeStable(new QLabel(this), "registerStatus"));

    auto* form = new QFormLayout;
    form->addRow(userLabel, usernameField);
    form->addRow(passLabel, passwordField);

    auto* loginRow = new QVBoxLayout;
    loginRow->addWidget(loginButton);
    loginRow->addWidget(signinStatus);

    auto* registerRow = new QVBoxLayout;
    registerRow->addWidget(registerButton);
    registerRow->addWidget(registerStatus);

    auto* top = new QVBoxLayout(this);
    top->addLayout(form);
    top->addLayout(loginRow);
    top->addLayout(registerRow);
    top->addWidget(registerToggle);

    connect(loginButton, &QPushButton::clicked, this, &SignInDialog::onLogin);
    connect(registerButton, &QPushButton::clicked, this, &SignInDialog::onRegister);
    connect(registerToggle, &QPushButton::clicked, this, &SignInDialog::onRegisterToggle);

    showLoginMode();
}

void SignInDialog::showLoginMode()
{
    registerMode = false;
    loginButton->setVisible(true);
    signinStatus->setVisible(true);
    registerButton->setVisible(false);
    registerStatus->setVisible(false);
    registerToggle->setText(tr("Create account"));
    signinStatus->clear();
    usernameField->clear();
    passwordField->clear();
}

void SignInDialog::onRegisterToggle()
{
    if (registerMode) {
        showLoginMode();
        return;
    }
    registerMode = true;
    loginButton->setVisible(false);
    signinStatus->setVisible(false);
    registerButton->setVisible(true);
    registerStatus->setVisible(true);
    registerToggle->setText(tr("Back to log in"));
    registerStatus->clear();
    usernameField->clear();
    passwordField->clear();
}

void SignInDialog::onLogin()
{
    const QString username = usernameField->text();
    const QString password = passwordField->text();

    if (!Db::usernames().contains(username)) {
        signinStatus->setText(QString::fromLatin1(kUnknownUsername));
        return;
    }
    if (!Db::authUser(username, password)) {
        signinStatus->setText(QString::fromLatin1(kWrongPassword));
        return;
    }
    user_ = username;
    emit acceptedWithUser(user_);
    accept();
}

void SignInDialog::onRegister()
{
    const QString username = usernameField->text();
    const QString password = passwordField->text();

    if (username.isEmpty()) {
        registerStatus->setText(QString::fromLatin1(kUsernameEmpty));
        return;
    }
    if (password.isEmpty()) {
        registerStatus->setText(QString::fromLatin1(kPasswordEmpty));
        return;
    }
    if (!Db::registerUser(username, password)) {
        registerStatus->setText(QString::fromLatin1(kUsernameExists));
        return;
    }
    // Success: stay in the dialog (owner ruling: register then log in).
    registerStatus->setText(QString::fromLatin1(kRegistered));
    usernameField->clear();
    passwordField->clear();
}
