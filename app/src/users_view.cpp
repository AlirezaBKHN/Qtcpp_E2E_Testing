#include "users_view.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>

#include "db.h"

namespace {

QWidget* makeStable(QWidget* widget, const char* id)
{
    widget->setObjectName(QString::fromLatin1(id));
    widget->setAccessibleName(QString::fromLatin1(id));
    return widget;
}

} // namespace

UsersView::UsersView(const QString& currentUser, QWidget* parent)
    : QWidget(parent)
    , currentUser_(currentUser)
{
    usersTable = qobject_cast<QTableWidget*>(
        makeStable(new QTableWidget(0, 1, this), "usersTable"));
    usersTable->setHorizontalHeaderLabels({tr("Username")});
    usersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* removeBtn = new QPushButton(tr("Remove user"), this);
    removeUserButton = qobject_cast<QPushButton*>(makeStable(removeBtn, "removeUserButton"));
    removeUserButton->setEnabled(currentUser_ == QLatin1String("admin"));

    auto* row = new QHBoxLayout;
    row->addWidget(removeUserButton);
    row->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(usersTable, 1);
    layout->addLayout(row);

    connect(removeUserButton, &QPushButton::clicked, this, &UsersView::onRemoveUser);

    reload();
}

void UsersView::reload()
{
    const QStringList names = Db::usernames();
    usersTable->setRowCount(0);
    for (const QString& name : names) {
        const int r = usersTable->rowCount();
        usersTable->insertRow(r);
        usersTable->setItem(r, 0, new QTableWidgetItem(name));
    }
}

void UsersView::onRemoveUser()
{
    const int row = usersTable->currentRow();
    if (row < 0)
        return;
    const QString name = usersTable->item(row, 0)->text();
    if (name == QLatin1String("admin"))
        return; // view-level self-delete guard additional to Db
    Db::removeUser(name);
    reload();
}
