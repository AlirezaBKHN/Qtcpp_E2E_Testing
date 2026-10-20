# TC015 — Users (admin only): the admin reaches the Users tab via its home
# card, removes a regular user, and the app guard refuses to remove admin
# (the row REMAINS; the guard itself is pinned by smoke).
def test_tc015_users_admin(admin):
    users = admin.users()  # raises unless the Users tab is mounted (admin)

    users.remove("guest")
    assert "guest" not in users.list(), "guest must be removed"

    users.remove("admin")  # app guard refuses: nothing happens
    assert "admin" in users.list(), "admin row must remain"