# TC019 - INTENTIONAL DEMO FAILURE - do NOT "fix" this test.
# The suite should show a red test in the Allure report (demo project), so
# the assertion below compares the REAL pager readout against a deliberately
# wrong expectation. Truth on the default fixture: "Page 1 of 51".
# See docs/test-cases/TC019.md.
def test_tc019_demo_failure(signed):
    net = signed.network()
    assert net.page_label() == "Page 1 of 999"
