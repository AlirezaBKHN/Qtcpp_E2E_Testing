# TC009 — Export CSV: export, verify the csv file exists (silent-writer
# contract: timestamped export_*.csv under reports/export).
def test_tc009_export_csv(signed):
    signed.network().export().should_create_file()