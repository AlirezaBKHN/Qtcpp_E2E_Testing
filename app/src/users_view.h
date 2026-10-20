#pragma once

#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QWidget>

// Admin-only Users tab: sorted username list + remove button. The user tab
// itself is only mounted for the literal 'admin' account, but the view
// keeps its controls when constructed for others (button disabled) so the
// contract stays testable. Removing 'admin' is refused here as well as in
// Db (double guard, owner ruling).
class UsersView : public QWidget
{
    Q_OBJECT

public:
    explicit UsersView(const QString& currentUser, QWidget* parent = nullptr);

private slots:
    void onRemoveUser();

private:
    void reload();

    QTableWidget* usersTable = nullptr;
    QPushButton* removeUserButton = nullptr;
    QString currentUser_;
};
