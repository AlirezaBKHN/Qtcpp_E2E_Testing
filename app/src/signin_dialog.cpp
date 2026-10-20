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

// READOUT dictation: a read-only, frameless QLineEdit whose objectName (and
// therefore UIA AutomationId) keeps the stable id, while the accessibleName
// mirrors the DISPLAYED TEXT. Probed live (Appium/WAD): no attribute key
// (Value/HelpText/Text/...) can ever read a QLineEdit's content, but
// get_attribute("Name") always returns the accessibleName - so the only
// readable surface for arbitrary readout copy is the Name itself.
QLineEdit* makeStatusReadout(QWidget* parent, const char* id)
{
    auto* field = new QLineEdit(parent);
    field->setReadOnly(true);
    field->setFrame(false);
    field->setObjectName(QString::fromLatin1(id));
    field->setAccessibleName(QString());
    return field;
}

// Every write to a readout MUST funnel through here: the automation stack
// reads the status copy through the UIA Name (== accessibleName).
void setReadoutText(QLineEdit* readout, const QString& text)
{
    readout->setText(text);
    readout->setAccessibleName(text);
}

// Pinned copy (plan Task 3): these strings are part of the automation
// contract and must not drift. The register-success copy carries a REAL
// em dash: a char[] with \xE2\x80\x94 bytes fed through QString::fromLatin1
// produced a broken 'â\x80\x94' in the UIA Name the automation stack
// reads, so this one is a QString constant (single U+2014 code point).
constexpr char kWrongPassword[] = "Wrong password.";
constexpr char kUnknownUsername[] = "Unknown username.";
constexpr char kUsernameExists[] = "Username already exists.";
constexpr char kPasswordEmpty[] = "Password can not be empty.";
constexpr char kUsernameEmpty[] = "Username can not be empty.";
const QString kRegistered = QStringLiteral("Registered \u2014 log in.");

} // namespace

SignInDialog::SignInDialog(QWidget* parent)
    : QDialog(parent)
{
    // The sign-in dialog IS the first screen: production shows it before
    // any MainWindow exists (probed: without this the default DB
    // connection is absent at first paint, so every login answers "Unknown
    // username." and register always reports "Username already exists.").
    // Ensuring here makes the dialog self-sufficient; Db::init's same-path
    // guard keeps repeat calls idempotent.
    Db::init(Db::defaultDatabasePath());

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

    signinStatus = makeStatusReadout(this, "signinStatus");
    registerStatus = makeStatusReadout(this, "registerStatus");

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
    setReadoutText(signinStatus, QString());
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
    setReadoutText(registerStatus, QString());
    usernameField->clear();
    passwordField->clear();
}

void SignInDialog::onLogin()
{
    const QString username = usernameField->text();
    const QString password = passwordField->text();

    if (!Db::usernames().contains(username)) {
        setReadoutText(signinStatus, QString::fromLatin1(kUnknownUsername));
        return;
    }
    if (!Db::authUser(username, password)) {
        setReadoutText(signinStatus, QString::fromLatin1(kWrongPassword));
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
        setReadoutText(registerStatus, QString::fromLatin1(kUsernameEmpty));
        return;
    }
    if (password.isEmpty()) {
        setReadoutText(registerStatus, QString::fromLatin1(kPasswordEmpty));
        return;
    }
    if (!Db::registerUser(username, password)) {
        setReadoutText(registerStatus, QString::fromLatin1(kUsernameExists));
        return;
    }
    // Success: register-first UX - the dialog switches back to LOGIN mode
    // in place with the pinned copy visible there (fields cleared; the user
    // then logs in). No auto-login.
    registerMode = false;
    loginButton->setVisible(true);
    signinStatus->setVisible(true);
    registerButton->setVisible(false);
    registerStatus->setVisible(false);
    registerToggle->setText(tr("Create account"));
    setReadoutText(signinStatus, kRegistered);
    usernameField->clear();
    passwordField->clear();
}
