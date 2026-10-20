# TC001 — Launch: the sign-in gate appears, and the register-first loop
# lands on the tabbed main window (register new user -> login -> home
# tab). DB isolation restores the store afterwards.
#
# NOTE: after a register SUCCESS the dialog is already back on the LOGIN
# screen (probed: a FAILED register stays in register mode, a success
# switches back) - no extra toggle before login().
def test_tc001_launch(tracker):
    tracker.register("assert_login_user", "tc001pass")
    assert tracker.register_message() == "Registered — log in."

    tracker.login("assert_login_user", "tc001pass")

    # The top-bar readout proves the tabbed main window is up (home tab).
    assert tracker.current_user() == "user: assert_login_user"