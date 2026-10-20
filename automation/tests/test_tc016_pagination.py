# TC016 — Pagination: default page ('Page 1 of 51'), prev/next with the
# realized id window cross-checked against the CSV export (full ordered
# result), page-size selector, 1000-size step LAST (UIA tree regrowth).
#
# UIA realizes only the VISIBLE rows of a page (~10 in this layout) - the
# pinned WAD quirk "UIA virtualizes table rows; never count DataItems".
# So the page-size assertions ride on the readout label math ("Page k of
# n" over the full query), while the realized id window is cross-checked
# against the export slice (first realized row of page k = export row
# (k-1)*size).
import csv

LARGE = "1000"  # selected LAST: regrows the UIA tree ~7x


def test_tc016_pagination(signed):
    net = signed.network()
    assert net.page_label() == "Page 1 of 51"

    net.next_page()
    assert net.page_label() == "Page 2 of 51"
    result = net.export().should_create_file()
    with open(result.newest_file(), newline="", encoding="utf-8") as fh:
        export_ids = [int(r["id"]) for r in csv.DictReader(fh)]
    assert net.table().realized_row_count() > 0  # sanity: page realized
    page_ids = net.table().ids()
    # First realized row of page 2 = export row 200; the whole realized
    # window must slice the export in order.
    assert page_ids[0] == export_ids[200]
    assert page_ids == export_ids[200:200 + len(page_ids)]

    net.prev_page()
    assert net.page_label() == "Page 1 of 51"

    net.page_size("50")
    assert net.page_label() == "Page 1 of 202"
    page_ids = net.table().ids()
    assert page_ids[0] == export_ids[0]
    assert page_ids == export_ids[0:len(page_ids)]

    net.page_size("200")
    assert net.page_label() == "Page 1 of 51"

    net.page_size(LARGE)
    assert net.page_label() == "Page 1 of 11"
    page_ids = net.table().ids()
    assert page_ids[0] == export_ids[0]
    assert page_ids == export_ids[0:len(page_ids)]
