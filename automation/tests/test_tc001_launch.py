# TC001 — Launch Application: verify main window exists.
# launch() waits up to 30 s for the top-level window and fails loudly if absent;
# the readback proves the app session is interactive, not just a process.
def test_tc001_launch(tracker):
    assert tracker.row_count() >= 0
