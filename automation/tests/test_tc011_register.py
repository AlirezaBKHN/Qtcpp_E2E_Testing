# TC011 — Register: a fresh name succeeds with the pinned copy; a duplicate
# name and an empty password fail with theirs. Assertions live in the TEST
# (the DSL only performs and reads back).
#
# Register-mode state machine (probed): after a SUCCESS the dialog returns
# to the LOGIN screen (the copy lands on the login readout), while a FAILED
# register stays in register mode. register() always re-enters register
# mode via its toggle, so it is only callable from the login screen.
def test_tc011_register(tracker):
    tracker.register("tc011_fresh", "tc011pass")
    assert tracker.register_message() == "Registered — log in."

    # Duplicate name (register() re-enters register mode; the failure
    # keeps the dialog there).
    tracker.register("tc011_fresh", "another")
    assert tracker.register_message() == "Username already exists."

    # Empty password: cycle back through the login screen and re-enter
    # register mode via register()'s own toggle.
    tracker.toggle_register_mode()
    tracker.register("tc011_fresh", "")
    assert tracker.register_message() == "Password can not be empty."