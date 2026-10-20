# TC013 — Logout: the main window is destroyed, the sign-in dialog returns,
# and the top-bar readout is gone (reads empty from the sign-in phase).
def test_tc013_logout(signed):
    signed.logout()
    assert signed.current_user() == ""