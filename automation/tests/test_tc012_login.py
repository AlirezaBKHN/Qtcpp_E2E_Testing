# TC012 — Login: wrong password and unknown username show the pinned copy
# on the sign-in dialog; the seeded account logs in to the main window.
def test_tc012_login(tracker):
    tracker.login("e2e_user", "wrong-password")
    assert tracker.signin_message() == "Wrong password."

    tracker.login("no_such_user", "whatever")
    assert tracker.signin_message() == "Unknown username."

    tracker.login("e2e_user", "e2e123")
    assert tracker.current_user() == "user: e2e_user"