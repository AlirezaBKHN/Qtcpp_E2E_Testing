"""Raw JSONWP probe of WinAppDriver: does the Root session see desktop windows?

Run inside the WAD's own session (spawned from this shell): if it lists
the owner's visible windows, this shell CAN drive the desktop.
"""
import json
import urllib.error
import urllib.parse
import urllib.request

URL = "http://127.0.0.1:4729"


def post(path, payload):
    req = urllib.request.Request(
        URL + path, data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"})
    try:
        return json.load(urllib.request.urlopen(req, timeout=60))
    except urllib.error.HTTPError as e:
        return {"__http": e.code, "body": e.read().decode()[:300]}


def get(path):
    req = urllib.request.Request(URL + path)
    try:
        return json.load(urllib.request.urlopen(req, timeout=60))
    except urllib.error.HTTPError as e:
        return {"__http": e.code, "body": e.read().decode()[:300]}


def main():
    data = post("/session", {"desiredCapabilities": {"app": "Root"}})
    sid = data.get("sessionId")
    if not sid:
        print("no session:", data)
        return 1
    q = None  # POST body carries using/value for JSONWP
    els = post(f"/session/{sid}/elements", {"using": "xpath", "value": "//Window"})
    if "__http" in els:
        print("elements failed:", els)
        return 1
    print("root-element find_elements:", json.dumps(els)[:200])
    names = []
    for el in els.get("value", [])[:15]:
        eid = el.get("ELEMENT")
        att = get(f"/session/{sid}/element/{eid}/attribute/name")
        names.append(att.get("value") or att.get("__http"))
    print("top-level windows visible to WAD:", len(els.get("value", [])))
    for n in names:
        print(" -", n)
    # cleanup
    try:
        urllib.request.urlopen(
            urllib.request.Request(f"{URL}/session/{sid}", method="DELETE"),
            timeout=30).read()
    except Exception:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
