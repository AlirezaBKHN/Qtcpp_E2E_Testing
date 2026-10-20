# TC018 - empty fixture: the mapped database has the seeded USERS (sign-in
# works) but ZERO cells: empty table, 'Page 1 of 1', 'Showing 0 rows'.
def test_tc018_empty_fixture(signed):
    net = signed.network()
    assert net.page_label() == "Page 1 of 1"
    assert net.row_count() == 0
    assert net.table().realized_row_count() == 0
