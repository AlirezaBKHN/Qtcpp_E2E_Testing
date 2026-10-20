# TC010 — Chart Visibility: verify the average-signal chart is displayed
# on the network tab.
def test_tc010_chart_visibility(signed):
    signed.network().chart().should_be_visible()