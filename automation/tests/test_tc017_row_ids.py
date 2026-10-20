# TC017 — Row-id DSL: should_contain_row / should_not_contain_row over the
# realized ID column; an unknown id must FAIL should_contain_row.
import csv

import pytest


def test_tc017_row_ids(signed):
    net = signed.network()
    result = net.export().should_create_file()
    with open(result.newest_file(), newline="", encoding="utf-8") as fh:
        export_ids = [int(r["id"]) for r in csv.DictReader(fh)]

    page1_ids = net.table().ids()
    assert export_ids[0] in page1_ids  # sanity: page 1 holds the export head

    net.table().should_contain_row(id=export_ids[0])
    net.next_page()
    net.table().should_not_contain_row(id=export_ids[0])
    net.table().should_contain_row(id=export_ids[200])

    net.prev_page()
    with pytest.raises(AssertionError):
        net.table().should_contain_row(id=99999)  # no such cell id
