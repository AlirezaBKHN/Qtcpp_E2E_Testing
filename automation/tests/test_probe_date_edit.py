# PROBE (plan 2026-10-20-dashboard-report-filters-charts, Task 7) — pin the
# WAD keystroke recipe for QDateEdit. NOT in suites.json: this is the
# recorded evidence that element-targeted send_keys digits land the typed
# date (fresh-state probe: focus starts at the YEAR section and the 8
# digits auto-advance year->month->day). The recipe lives in
# pageobjects.dashboard_view.set_date; the TCs (TC020+) consume it through
# the DSL.
def test_probe_date_edit(admin):
    dash = admin.dashboard()  # routes via homeCardDashboard

    before_from = dash.from_date_text()
    before_to = dash.to_date_text()
    # Sanity: the readouts expose the displayed era-default dates.
    assert before_from != "" and before_to != "", (
        f"date readouts unreadable: from={before_from!r} to={before_to!r}")

    dash._view.set_date("dashboardFromDateEdit", "2026-01-15")
    assert dash.from_date_text() == "2026-01-15", (
        f"typed 2026-01-15, readout shows {dash.from_date_text()!r}")

    dash._view.set_date("dashboardToDateEdit", "2026-01-20")
    assert dash.to_date_text() == "2026-01-20", (
        f"typed 2026-01-20, readout shows {dash.to_date_text()!r}")

    # The from-date must survive typing the other edit (focus moves).
    assert dash.from_date_text() == "2026-01-15"