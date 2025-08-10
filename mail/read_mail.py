import os, imaplib, email, json, csv, re, getpass
from email.header import decode_header
from datetime import datetime

IMAP_SERVER = "imap.gmail.com"

# --- Config ---
MAILBOX = "INBOX"      # e.g., "INBOX", "[Gmail]/All Mail", "[Gmail]/Spam"
LIMIT   = 50           # how many latest emails to fetch
SEARCH  = 'ALL'        # IMAP search query, e.g., 'UNSEEN', 'SINCE 1-Feb-2025'
OUT_JSONL = "emails.jsonl"
OUT_CSV   = "emails.csv"
# -------------


def _dh(value):
    """Decode RFC2047 headers safely to str."""
    if value is None:
        return ""
    parts = decode_header(value)
    decoded = []
    for text, enc in parts:
        if isinstance(text, bytes):
            try:
                decoded.append(text.decode(enc or "utf-8", errors="replace"))
            except Exception:
                decoded.append(text.decode("utf-8", errors="replace"))
        else:
            decoded.append(text)
    return "".join(decoded)


def _get_text_from_msg(msg):
    """Prefer text/plain; if missing, lightly strip HTML."""
    if msg.is_multipart():
        # walk() yields parts depth-first; prefer text/plain without attachment
        for part in msg.walk():
            ctype = (part.get_content_type() or "").lower()
            cd = (part.get("Content-Disposition") or "").lower()
            if "attachment" in cd:
                continue
            if ctype == "text/plain":
                try:
                    return part.get_payload(decode=True).decode(part.get_content_charset() or "utf-8", "replace")
                except Exception:
                    return part.get_payload(decode=True).decode("utf-8", "replace")
        # fallback: first text/html
        for part in msg.walk():
            ctype = (part.get_content_type() or "").lower()
            if ctype == "text/html":
                html = part.get_payload(decode=True).decode(part.get_content_charset() or "utf-8", "replace")
                return _light_html_to_text(html)
    else:
        payload = msg.get_payload(decode=True)
        if payload is None:
            return ""
        try:
            return payload.decode(msg.get_content_charset() or "utf-8", "replace")
        except Exception:
            return payload.decode("utf-8", "replace")
    return ""


def _light_html_to_text(html):
    # minimal & fast: drop tags and collapse whitespace
    text = re.sub(r"(?is)<(script|style).*?>.*?</\1>", " ", html)
    text = re.sub(r"(?s)<br\s*/?>", "\n", text)
    text = re.sub(r"(?s)</p\s*>", "\n", text)
    text = re.sub(r"(?s)<.*?>", " ", text)
    text = re.sub(r"&nbsp;", " ", text)
    text = re.sub(r"\s+\n", "\n", text)
    text = re.sub(r"\n\s+", "\n", text)
    return re.sub(r"[ \t]+", " ", text).strip()


def _snippet(text, n=160):
    text = text.replace("\r", "").replace("\n", " ")
    return (text[:n] + "…") if len(text) > n else text


def fetch_latest():
    email_addr = os.getenv("GMAIL_ADDRESS") or input("Gmail address: ").strip()
    app_pwd    = os.getenv("GMAIL_APP_PASSWORD") or getpass.getpass("App password (16 chars): ")

    print("Connecting to IMAP…")
    m = imaplib.IMAP4_SSL(IMAP_SERVER)
    m.login(email_addr, app_pwd)
    m.select(MAILBOX)

    # Search and choose last LIMIT UIDs
    status, data = m.search(None, SEARCH)
    if status != "OK":
        raise RuntimeError(f"IMAP search failed: {status} {data}")

    all_ids = data[0].split()
    if not all_ids:
        print("No messages match search.")
        return []

    ids = all_ids[-LIMIT:]  # newest last; we’ll fetch in that order
    results = []

    print(f"Fetching {len(ids)} emails from '{MAILBOX}' (query: {SEARCH})…")
    for i, eid in enumerate(ids, 1):
        status, msg_data = m.fetch(eid, "(RFC822 INTERNALDATE)")
        if status != "OK":
            print(f"  ! fetch {eid} failed: {status}")
            continue

        raw = None
        internaldate = None
        for part in msg_data:
            if isinstance(part, tuple):
                # part[0] contains metadata; part[1] is raw RFC822
                raw = part[1]
                # try to parse INTERNALDATE from the metadata line
                meta = part[0].decode(errors="ignore")
                # INTERNALDATE "dd-Mon-YYYY hh:mm:ss +0000"
                m_dt = re.search(r'INTERNALDATE "([^"]+)"', meta)
                if m_dt:
                    internaldate = m_dt.group(1)
        if raw is None:
            continue

        msg = email.message_from_bytes(raw)
        frm = _dh(msg.get("From"))
        to  = _dh(msg.get("To"))
        cc  = _dh(msg.get("Cc"))
        sub = _dh(msg.get("Subject"))
        date_hdr = _dh(msg.get("Date"))
        body = _get_text_from_msg(msg)

        # Try normalize date to ISO if possible
        iso_date = None
        try:
            # email.utils.parsedate_to_datetime is nice but 3.6 compatibility varies
            from email.utils import parsedate_to_datetime
            iso_date = parsedate_to_datetime(date_hdr).isoformat()
        except Exception:
            iso_date = None

        entry = {
            "uid": eid.decode(),
            "from": frm,
            "to": to,
            "cc": cc,
            "subject": sub,
            "date_header": date_hdr,
            "date_iso": iso_date,
            "internaldate": internaldate,
            "snippet": _snippet(body),
            "body": body,
        }
        results.append(entry)
        print(f"  [{i}/{len(ids)}] {sub or '(no subject)'} — {frm}")

    m.close()
    m.logout()
    return results


def save_jsonl(items, path):
    with open(path, "w", encoding="utf-8") as f:
        for it in items:
            f.write(json.dumps(it, ensure_ascii=False) + "\n")


def save_csv(items, path):
    fields = ["uid", "from", "to", "cc", "subject", "date_iso", "date_header", "internaldate", "snippet"]
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for it in items:
            row = {k: it.get(k, "") for k in fields}
            w.writerow(row)


if __name__ == "__main__":
    emails = fetch_latest()
    if emails:
        save_jsonl(emails, OUT_JSONL)
        save_csv(emails, OUT_CSV)
        print(f"\nSaved {len(emails)} emails to:\n  • {OUT_JSONL}\n  • {OUT_CSV}")
    else:
        print("Nothing saved.")
