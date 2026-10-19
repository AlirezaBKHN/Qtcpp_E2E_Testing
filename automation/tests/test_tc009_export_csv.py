# TC009 — Export CSV: export, verify csv file exists.
def test_tc009_export_csv(tracker):
    tracker.export().should_create_file()
