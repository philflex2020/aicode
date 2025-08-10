Here’s the **estimated 6-month return ranges** we discussed for your two \$1,000 strategies:

---

## **1️⃣ High-Risk Strategy** (Leveraged ETFs, emerging markets, crypto)

* **Best case**: **+40% to +100%** → \$1,400 – \$2,000
* **Worst case**: **–40% to –70%** → \$600 – \$300
* Highly volatile; daily swings of 5–10% possible.
* Returns heavily depend on market trend staying favorable for tech, emerging markets, and crypto.

---

## **2️⃣ Medium-Risk Strategy** (Large-cap dividend ETFs, S\&P 500 ETF, bonds)

* **Best case**: **+8% to +15%** → \$1,080 – \$1,150
* **Worst case**: **–5% to –12%** → \$950 – \$880
* Much steadier; smaller daily moves; bond component cushions declines.

---

If you want, I can also **model probability distributions** for these ranges based on the past 10 years of daily returns for each ETF/asset.
That would give you a **risk-adjusted expectation** instead of just a high/low scenario.

Do you want me to run that simulation for you?


gotcha — let’s do a **Python-based tracker** you can run locally.
It’ll pull live prices (including crypto) with `yfinance`, compute P\&L, and flag stop-loss / take-profit hits for your **High-Risk** and **Medium-Risk** portfolios.

### What you get

* Read positions from a simple JSON file (two strategies).
* Live prices for stocks/ETFs **and** crypto (`ETH-USD`, `BTC-USD`) via `yfinance`.
* Calculates current value, % gain/loss, and whether stop-loss / take-profit levels are hit.
* Exports a dated CSV snapshot + prints a clean table to the console.

---

### 1) Install deps

```bash
pip install yfinance pandas tabulate
```

---

### 2) Create `positions.json`

```json
{
  "High_Risk": [
    {
      "name": "Leveraged Nasdaq",
      "ticker": "TQQQ",
      "entry_price": 50.00,
      "quantity": 12.0,
      "stop_loss_pct": -0.15,
      "take_profit_pct": 0.40
    },
    {
      "name": "Emerging Mkts",
      "ticker": "EEM",
      "entry_price": 40.00,
      "quantity": 5.0,
      "stop_loss_pct": -0.12,
      "take_profit_pct": 0.25
    },
    {
      "name": "Ethereum",
      "ticker": "ETH-USD",
      "entry_price": 3200.00,
      "quantity": 0.0625,
      "stop_loss_pct": -0.20,
      "take_profit_pct": 0.50
    }
  ],
  "Medium_Risk": [
    {
      "name": "Dividend ETF",
      "ticker": "SCHD",
      "entry_price": 75.00,
      "quantity": 5.3333,
      "stop_loss_pct": -0.10,
      "take_profit_pct": 0.15
    },
    {
      "name": "S&P 500 ETF",
      "ticker": "VOO",
      "entry_price": 500.00,
      "quantity": 0.8,
      "stop_loss_pct": -0.10,
      "take_profit_pct": 0.15
    },
    {
      "name": "ST Corp Bonds",
      "ticker": "VCSH",
      "entry_price": 80.00,
      "quantity": 2.5,
      "stop_loss_pct": null,
      "take_profit_pct": null
    }
  ]
}
```

> Tip: Use your actual fills for `entry_price` and `quantity`. If you prefer allocations instead, I can switch the script to compute quantity from dollar allocation.

---

### 3) Save `tracker.py`

```python
import argparse
import datetime as dt
import json
import sys
from typing import Dict, List

import pandas as pd
import yfinance as yf
from tabulate import tabulate


def load_positions(path: str) -> Dict[str, List[dict]]:
    with open(path, "r") as f:
        data = json.load(f)
    # Basic validation
    if not isinstance(data, dict) or "High_Risk" not in data or "Medium_Risk" not in data:
        raise ValueError("positions.json must contain 'High_Risk' and 'Medium_Risk' keys.")
    return data


def fetch_prices(tickers: List[str]) -> Dict[str, float]:
    # yfinance can fetch multiple tickers; falls back gracefully
    unique = sorted(set(tickers))
    prices = {}
    # Fast path for single ticker
    if len(unique) == 1:
        t = unique[0]
        prices[t] = float(yf.Ticker(t).fast_info.last_price)
        return prices

    data = yf.download(unique, period="1d", interval="1m", progress=False, threads=True)
    # data['Close'] shape varies for one vs many tickers
    if "Close" in data:
        close = data["Close"]
        if isinstance(close, pd.Series):
            # single ticker series
            last = close.dropna().iloc[-1]
            prices[unique[0]] = float(last)
        else:
            # dataframe for many tickers
            last_row = close.dropna(how="all").iloc[-1]
            for t in unique:
                val = last_row.get(t)
                if pd.isna(val):
                    # fallback using fast_info
                    val = yf.Ticker(t).fast_info.last_price
                prices[t] = float(val)
    else:
        # fallback path
        for t in unique:
            prices[t] = float(yf.Ticker(t).fast_info.last_price)
    return prices


def evaluate_strategy(name: str, positions: List[dict], live_prices: Dict[str, float]) -> pd.DataFrame:
    rows = []
    for p in positions:
        ticker = p["ticker"]
        entry = float(p["entry_price"])
        qty = float(p["quantity"])
        price = float(live_prices.get(ticker, entry))  # fallback to entry if missing
        cost = entry * qty
        value = price * qty
        pnl = value - cost
        pnl_pct = (price / entry - 1.0) if entry > 0 else 0.0

        stop_loss_pct = p.get("stop_loss_pct", None)
        take_profit_pct = p.get("take_profit_pct", None)
        stop_price = entry * (1.0 + stop_loss_pct) if isinstance(stop_loss_pct, (int, float)) else None
        tp_price = entry * (1.0 + take_profit_pct) if isinstance(take_profit_pct, (int, float)) else None

        status = "Active"
        if stop_price is not None and price <= stop_price:
            status = "Hit STOP"
        if tp_price is not None and price >= tp_price:
            status = "Hit TAKE"

        rows.append({
            "Strategy": name,
            "Name": p.get("name", ""),
            "Ticker": ticker,
            "Qty": qty,
            "Entry": round(entry, 6),
            "Price": round(price, 6),
            "StopPx": (round(stop_price, 6) if stop_price is not None else ""),
            "TakePx": (round(tp_price, 6) if tp_price is not None else ""),
            "Cost": round(cost, 2),
            "Value": round(value, 2),
            "PnL": round(pnl, 2),
            "PnL%": round(pnl_pct * 100.0, 2),
            "Status": status,
        })
    df = pd.DataFrame(rows)
    return df


def summarize(df: pd.DataFrame) -> pd.DataFrame:
    if df.empty:
        return pd.DataFrame()
    by = df.groupby("Strategy", as_index=False).agg(
        Cost=("Cost", "sum"),
        Value=("Value", "sum"),
        PnL=("PnL", "sum")
    )
    by["PnL%"] = (by["PnL"] / by["Cost"]) * 100.0
    return by


def main():
    ap = argparse.ArgumentParser(description="Two-Portfolio Tracker (High/Medium Risk)")
    ap.add_argument("--positions", default="positions.json", help="Path to positions JSON")
    ap.add_argument("--export_csv", action="store_true", help="Export a CSV snapshot")
    args = ap.parse_args()

    try:
        data = load_positions(args.positions)
    except Exception as e:
        print(f"Error loading positions: {e}")
        sys.exit(1)

    tickers = []
    for k in ("High_Risk", "Medium_Risk"):
        for p in data.get(k, []):
            tickers.append(p["ticker"])

    try:
        live_prices = fetch_prices(tickers)
    except Exception as e:
        print(f"Error fetching prices: {e}")
        sys.exit(1)

    df_high = evaluate_strategy("High_Risk", data["High_Risk"], live_prices)
    df_med = evaluate_strategy("Medium_Risk", data["Medium_Risk"], live_prices)
    df_all = pd.concat([df_high, df_med], ignore_index=True)

    # Print detail tables
    if not df_high.empty:
        print("\n=== High_Risk ===")
        print(tabulate(df_high, headers="keys", tablefmt="github", showindex=False))
    if not df_med.empty:
        print("\n=== Medium_Risk ===")
        print(tabulate(df_med, headers="keys", tablefmt="github", showindex=False))

    # Print summary
    summary = summarize(df_all)
    if not summary.empty:
        print("\n=== Summary ===")
        print(tabulate(summary, headers="keys", tablefmt="github", showindex=False))

    # CSV export
    if args.export_csv:
        ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        out_detail = f"tracker_detail_{ts}.csv"
        out_summary = f"tracker_summary_{ts}.csv"
        df_all.to_csv(out_detail, index=False)
        summary.to_csv(out_summary, index=False)
        print(f"\nSaved: {out_detail}")
        print(f"Saved: {out_summary}")


if __name__ == "__main__":
    main()
```

---

### 4) Run it

```bash
python tracker.py --positions positions.json --export_csv
```

You’ll see nicely formatted tables for **High\_Risk**, **Medium\_Risk**, and a **Summary**, plus CSV snapshots dropped in your working directory.

---

If you want:

* automatic **trailing stops** (e.g., convert “Hit TAKE” to a 10% trailing stop suggestion),
* **allocation-based** entries (compute quantity from dollars), or
* a **Google Sheets uploader** (via `gspread`),

say the word and I’ll extend this script.
Awesome — here’s a single, ready-to-run Python tracker that adds all three upgrades:

* **Allocation-based entries** (auto-compute quantity from dollars)
* **Automatic take-profit → trailing stop** (with persistent state)
* **Google Sheets upload** (details + summary tabs)

---

# 1) Install deps

```bash
pip install yfinance pandas tabulate gspread oauth2client
```

---

# 2) Positions file (`positions.json`)

Supports either `quantity` **or** `allocation_usd`. If both are present, `quantity` wins.
Optional per-position `trail_pct` (default 0.10 = 10%).

```json
{
  "High_Risk": [
    {
      "name": "Leveraged Nasdaq",
      "ticker": "TQQQ",
      "entry_price": 50.00,
      "allocation_usd": 600,
      "stop_loss_pct": -0.15,
      "take_profit_pct": 0.40,
      "trail_pct": 0.10
    },
    {
      "name": "Emerging Mkts",
      "ticker": "EEM",
      "entry_price": 40.00,
      "allocation_usd": 200,
      "stop_loss_pct": -0.12,
      "take_profit_pct": 0.25
    },
    {
      "name": "Ethereum",
      "ticker": "ETH-USD",
      "entry_price": 3200.00,
      "allocation_usd": 200,
      "stop_loss_pct": -0.20,
      "take_profit_pct": 0.50,
      "trail_pct": 0.12
    }
  ],
  "Medium_Risk": [
    {
      "name": "Dividend ETF",
      "ticker": "SCHD",
      "entry_price": 75.00,
      "allocation_usd": 400,
      "stop_loss_pct": -0.10,
      "take_profit_pct": 0.15
    },
    {
      "name": "S&P 500 ETF",
      "ticker": "VOO",
      "entry_price": 500.00,
      "allocation_usd": 400,
      "stop_loss_pct": -0.10,
      "take_profit_pct": 0.15
    },
    {
      "name": "ST Corp Bonds",
      "ticker": "VCSH",
      "entry_price": 80.00,
      "allocation_usd": 200
    }
  ]
}
```

---

# 3) Google Sheets credentials

1. In Google Cloud Console, create a **Service Account** and download its **JSON** key file, e.g. `creds.json`.
2. Create a Google Sheet (or let the script create one).
3. Share the sheet with the service account’s email (from the JSON), **Editor** permission.

---

# 4) The script (`tracker.py`)

```python
import argparse
import datetime as dt
import json
import os
import sys
from typing import Dict, List, Tuple, Any

import pandas as pd
import yfinance as yf
from tabulate import tabulate

# Optional: Google Sheets
try:
    import gspread
    from oauth2client.service_account import ServiceAccountCredentials
    HAS_GS = True
except Exception:
    HAS_GS = False


# -----------------------
# I/O helpers
# -----------------------
def load_json(path: str) -> Any:
    with open(path, "r") as f:
        return json.load(f)


def save_json(path: str, data: Any) -> None:
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, indent=2)
    os.replace(tmp, path)


# -----------------------
# Positions & State
# -----------------------
def load_positions(path: str) -> Dict[str, List[dict]]:
    data = load_json(path)
    if not isinstance(data, dict) or "High_Risk" not in data or "Medium_Risk" not in data:
        raise ValueError("positions.json must contain 'High_Risk' and 'Medium_Risk' keys.")
    return data


def ensure_state(path: str, positions: Dict[str, List[dict]]) -> Dict[str, dict]:
    """
    State structure (per strategy+ticker):
      {
        "<strategy>|<ticker>": {
           "trailing_active": bool,
           "trail_pct": float,
           "peak_price": float,          # peak since trailing activation
           "take_trigger_price": float   # entry*(1+take_profit_pct) when activated
        },
        ...
      }
    """
    if os.path.exists(path):
        state = load_json(path)
    else:
        state = {}

    # ensure keys exist
    for strat in ("High_Risk", "Medium_Risk"):
        for p in positions.get(strat, []):
            key = f"{strat}|{p['ticker']}"
            state.setdefault(key, {
                "trailing_active": False,
                "trail_pct": float(p.get("trail_pct", 0.10)),
                "peak_price": None,
                "take_trigger_price": None
            })
            # Keep trail_pct updated from positions (if provided)
            if "trail_pct" in p:
                state[key]["trail_pct"] = float(p["trail_pct"])

    return state


# -----------------------
# Market data
# -----------------------
def fetch_prices(tickers: List[str]) -> Dict[str, float]:
    unique = sorted(set(tickers))
    prices: Dict[str, float] = {}

    # Try fast path for many tickers
    try:
        data = yf.download(unique, period="1d", interval="1m", progress=False, threads=True)
        if "Close" in data:
            close = data["Close"]
            if isinstance(close, pd.Series):
                last = close.dropna().iloc[-1]
                prices[unique[0]] = float(last)
            else:
                last_row = close.dropna(how="all").iloc[-1]
                for t in unique:
                    val = last_row.get(t)
                    if pd.isna(val):
                        val = yf.Ticker(t).fast_info.last_price
                    prices[t] = float(val)
        else:
            for t in unique:
                prices[t] = float(yf.Ticker(t).fast_info.last_price)
    except Exception:
        # Fallback to fast_info one-by-one
        for t in unique:
            prices[t] = float(yf.Ticker(t).fast_info.last_price)

    return prices


# -----------------------
# Core evaluation
# -----------------------
def compute_quantity(entry_price: float, pos: dict) -> float:
    # If quantity provided, use it
    if "quantity" in pos and pos["quantity"] is not None:
        return float(pos["quantity"])
    # Else compute from allocation
    alloc = float(pos.get("allocation_usd", 0.0))
    if alloc <= 0.0:
        return 0.0
    # allow fractional shares (6 decimals)
    return round(alloc / entry_price, 6)


def evaluate_positions(
    strategy_name: str,
    positions: List[dict],
    live_prices: Dict[str, float],
    state: Dict[str, dict],
    auto_activate_trailing: bool
) -> Tuple[pd.DataFrame, Dict[str, dict]]:
    rows = []
    for p in positions:
        ticker = p["ticker"]
        entry = float(p["entry_price"])
        price = float(live_prices.get(ticker, entry))
        qty = compute_quantity(entry, p)

        cost = entry * qty
        value = price * qty
        pnl = value - cost
        pnl_pct = (price / entry - 1.0) if entry > 0 else 0.0

        stop_loss_pct = p.get("stop_loss_pct", None)
        take_profit_pct = p.get("take_profit_pct", None)
        stop_px = entry * (1.0 + stop_loss_pct) if isinstance(stop_loss_pct, (int, float)) else None
        tp_px = entry * (1.0 + take_profit_pct) if isinstance(take_profit_pct, (int, float)) else None

        key = f"{strategy_name}|{ticker}"
        s = state.get(key, {
            "trailing_active": False,
            "trail_pct": float(p.get("trail_pct", 0.10)),
            "peak_price": None,
            "take_trigger_price": None
        })
        trail_pct = float(s.get("trail_pct", p.get("trail_pct", 0.10)))
        trailing_active = bool(s.get("trailing_active", False))
        peak_price = s.get("peak_price", None)
        take_trigger_price = s.get("take_trigger_price", None)

        status = "Active"
        trail_stop_px = ""

        # 1) Hard stop-loss check (if not trailing yet)
        if not trailing_active and stop_px is not None and price <= stop_px:
            status = "Hit STOP"

        # 2) Take-profit -> activate trailing (auto if requested)
        if take_profit_pct is not None and price >= tp_px:
            if auto_activate_trailing and not trailing_active:
                trailing_active = True
                peak_price = price
                take_trigger_price = tp_px
                status = "Trailing"
            elif not auto_activate_trailing and not trailing_active:
                status = "Hit TAKE (not trailing)"
            else:
                status = "Trailing"

        # 3) If trailing is active, update peak and compute trailing stop
        if trailing_active:
            if peak_price is None or price > peak_price:
                peak_price = price
            trail_stop = peak_price * (1.0 - trail_pct)
            trail_stop_px = round(trail_stop, 6)

            if price <= trail_stop:
                status = "Hit TRAIL STOP"
            else:
                # if we didn't already set status above, keep trailing
                if "Trailing" not in status and "TAKE" not in status:
                    status = "Trailing"

        # Persist state changes
        state[key] = {
            "trailing_active": trailing_active,
            "trail_pct": trail_pct,
            "peak_price": peak_price,
            "take_trigger_price": take_trigger_price
        }

        rows.append({
            "Strategy": strategy_name,
            "Name": p.get("name", ""),
            "Ticker": ticker,
            "Qty": round(qty, 6),
            "Entry": round(entry, 6),
            "Price": round(price, 6),
            "StopPx": (round(stop_px, 6) if stop_px is not None else ""),
            "TakePx": (round(tp_px, 6) if tp_px is not None else ""),
            "Trail%": round(trail_pct * 100.0, 2) if trailing_active or "trail_pct" in p else "",
            "TrailStopPx": trail_stop_px,
            "Cost": round(cost, 2),
            "Value": round(value, 2),
            "PnL": round(pnl, 2),
            "PnL%": round(pnl_pct * 100.0, 2),
            "Status": status
        })

    return pd.DataFrame(rows), state


def summarize(df: pd.DataFrame) -> pd.DataFrame:
    if df.empty:
        return pd.DataFrame()
    by = df.groupby("Strategy", as_index=False).agg(
        Cost=("Cost", "sum"),
        Value=("Value", "sum"),
        PnL=("PnL", "sum")
    )
    by["PnL%"] = (by["PnL"] / by["Cost"]).mul(100.0).round(2)
    return by


# -----------------------
# Google Sheets uploader
# -----------------------
def upload_to_sheets(creds_path: str, sheet_name: str, details_df: pd.DataFrame, summary_df: pd.DataFrame):
    if not HAS_GS:
        print("gspread / oauth2client not installed; skipping Google Sheets upload.")
        return

    scope = ["https://spreadsheets.google.com/feeds", "https://www.googleapis.com/auth/drive"]
    creds = ServiceAccountCredentials.from_json_keyfile_name(creds_path, scope)
    gc = gspread.authorize(creds)

    # Create or open sheet
    try:
        sh = gc.open(sheet_name)
    except gspread.SpreadsheetNotFound:
        sh = gc.create(sheet_name)

    # IMPORTANT: Make sure you shared the sheet with the service account email

    # Details tab
    details_title = "Details"
    if details_title in [ws.title for ws in sh.worksheets()]:
        ws_d = sh.worksheet(details_title)
        sh.del_worksheet(ws_d)
    ws_d = sh.add_worksheet(title=details_title, rows=str(len(details_df) + 10), cols=str(len(details_df.columns) + 5))

    # Summary tab
    summary_title = "Summary"
    if summary_title in [ws.title for ws in sh.worksheets()]:
        ws_s = sh.worksheet(summary_title)
        sh.del_worksheet(ws_s)
    ws_s = sh.add_worksheet(title=summary_title, rows=str(len(summary_df) + 10), cols=str(len(summary_df.columns) + 5))

    # Write data (as lists)
    if not details_df.empty:
        ws_d.update([details_df.columns.tolist()] + details_df.astype(str).values.tolist())
    if not summary_df.empty:
        ws_s.update([summary_df.columns.tolist()] + summary_df.astype(str).values.tolist())

    print(f"Uploaded to Google Sheets: {sheet_name} (Details & Summary)")


# -----------------------
# CLI
# -----------------------
def main():
    ap = argparse.ArgumentParser(description="Two-Portfolio Tracker (allocations, trailing stops, Google Sheets)")
    ap.add_argument("--positions", default="positions.json", help="Path to positions JSON")
    ap.add_argument("--state", default="tracker_state.json", help="Path to persistent state JSON")
    ap.add_argument("--export_csv", action="store_true", help="Export CSV snapshots")
    ap.add_argument("--auto_activate_trailing", action="store_true",
                    help="Auto-activate trailing stop once take-profit is reached")
    ap.add_argument("--sheet", default="", help="Google Sheet name to upload (optional)")
    ap.add_argument("--creds", default="", help="Service account creds JSON for Google Sheets")
    ap.add_argument("--force_reset_state", action="store_true", help="Reset state file (fresh trailing)")
    args = ap.parse_args()

    # Load positions
    try:
        positions = load_positions(args.positions)
    except Exception as e:
        print(f"Error loading positions: {e}")
        sys.exit(1)

    # State
    if args.force_reset_state and os.path.exists(args.state):
        os.remove(args.state)
    state = ensure_state(args.state, positions)

    # Fetch prices
    tickers = []
    for k in ("High_Risk", "Medium_Risk"):
        for p in positions.get(k, []):
            tickers.append(p["ticker"])
    try:
        live_prices = fetch_prices(tickers)
    except Exception as e:
        print(f"Error fetching prices: {e}")
        sys.exit(1)

    # Evaluate
    df_high, state = evaluate_positions("High_Risk", positions["High_Risk"], live_prices, state, args.auto_activate_trailing)
    df_med, state = evaluate_positions("Medium_Risk", positions["Medium_Risk"], live_prices, state, args.auto_activate_trailing)
    df_all = pd.concat([df_high, df_med], ignore_index=True)

    # Save state back
    save_json(args.state, state)

    # Print detail
    if not df_high.empty:
        print("\n=== High_Risk ===")
        print(tabulate(df_high, headers="keys", tablefmt="github", showindex=False))
    if not df_med.empty:
        print("\n=== Medium_Risk ===")
        print(tabulate(df_med, headers="keys", tablefmt="github", showindex=False))

    # Summary
    summary = summarize(df_all)
    if not summary.empty:
        print("\n=== Summary ===")
        print(tabulate(summary, headers="keys", tablefmt="github", showindex=False))

    # CSV export
    if args.export_csv:
        ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        out_detail = f"tracker_detail_{ts}.csv"
        out_summary = f"tracker_summary_{ts}.csv"
        df_all.to_csv(out_detail, index=False)
        summary.to_csv(out_summary, index=False)
        print(f"\nSaved: {out_detail}")
        print(f"Saved: {out_summary}")

    # Google Sheets upload
    if args.sheet:
        if not args.creds:
            print("No --creds provided; skipping Google Sheets upload.")
        else:
            upload_to_sheets(args.creds, args.sheet, df_all, summary)


if __name__ == "__main__":
    main()
```

---

# 5) Run it

Basic (console only):

```bash
python tracker.py --positions positions.json --auto_activate_trailing
```

Export CSVs too:

```bash
python tracker.py --positions positions.json --auto_activate_trailing --export_csv
```

Upload to Google Sheets:

```bash
python tracker.py --positions positions.json --auto_activate_trailing \
  --sheet "Two-Portfolio Tracker" --creds creds.json
```

Reset trailing state (fresh run):

```bash
python tracker.py --force_reset_state --auto_activate_trailing
```

---

## Notes & tweaks

* **Trailing stops** persist via `tracker_state.json`. Once TP is hit (and `--auto_activate_trailing` used), the script tracks **peak price** and computes a **dynamic stop** = `peak*(1-trail_pct)`.
* You can set a per-position `trail_pct`. If absent, defaults to **10%**.
* **Allocations**: if you prefer fixed share sizes, add a `quantity`; it overrides `allocation_usd`.
* Crypto tickers like `ETH-USD` and `BTC-USD` work via `yfinance`.

Want me to add **email/Discord alerts** on “Hit STOP/TAKE/TRAIL STOP”?
Sweet — let’s bolt **email alerts** onto your tracker so you get a message the moment a position hits **STOP / TAKE / TRAIL STOP**. Below are drop-in changes (no other libs needed; uses `smtplib`).

---

# 1) Add CLI flags (SMTP + recipients)

In `main()`’s argparse block, add:

```python
ap.add_argument("--smtp_server", default="", help="SMTP server hostname")
ap.add_argument("--smtp_port", type=int, default=587, help="SMTP port (587 TLS, 465 SSL)")
ap.add_argument("--smtp_user", default="", help="SMTP username")
ap.add_argument("--smtp_pass", default="", help="SMTP password or app password")
ap.add_argument("--email_from", default="", help="From email address")
ap.add_argument("--email_to", default="", help="Comma-separated recipient emails")
ap.add_argument("--email_ssl", action="store_true", help="Use SSL instead of STARTTLS")
ap.add_argument("--email_subject_prefix", default="[Tracker]", help="Email subject prefix")
```

---

# 2) Track last notified status (state schema)

In `ensure_state(...)`, add a default `last_notified_status` and keep it persisted:

```python
state.setdefault(key, {
    "trailing_active": False,
    "trail_pct": float(p.get("trail_pct", 0.10)),
    "peak_price": None,
    "take_trigger_price": None,
    "last_notified_status": ""     # <— NEW
})
# keep trail_pct fresh:
if "trail_pct" in p:
    state[key]["trail_pct"] = float(p["trail_pct"])
```

And when you update `state[key]` in `evaluate_positions(...)`, include:

```python
"last_notified_status": state.get(key, {}).get("last_notified_status", "")
```

(We’ll actually overwrite `last_notified_status` later **after** sending emails.)

---

# 3) Email helper

Add near the top-level helpers:

```python
import smtplib
from email.mime.text import MIMEText
from email.utils import formatdate

ALERT_STATUSES = {"Hit STOP", "Hit TAKE", "Hit TRAIL STOP"}

def send_email(smtp_cfg: dict, subject: str, body: str):
    if not smtp_cfg.get("server") or not smtp_cfg.get("from") or not smtp_cfg.get("to"):
        print("Email not configured, skipping alert.")
        return
    msg = MIMEText(body, "plain", "utf-8")
    msg["Subject"] = subject
    msg["From"] = smtp_cfg["from"]
    msg["To"] = smtp_cfg["to"]
    msg["Date"] = formatdate(localtime=True)

    try:
        if smtp_cfg.get("ssl"):
            with smtplib.SMTP_SSL(smtp_cfg["server"], smtp_cfg["port"]) as s:
                if smtp_cfg.get("user"):
                    s.login(smtp_cfg["user"], smtp_cfg["pass"])
                s.send_message(msg)
        else:
            with smtplib.SMTP(smtp_cfg["server"], smtp_cfg["port"]) as s:
                s.ehlo()
                s.starttls()
                s.ehlo()
                if smtp_cfg.get("user"):
                    s.login(smtp_cfg["user"], smtp_cfg["pass"])
                s.send_message(msg)
        print("Alert email sent.")
    except Exception as e:
        print(f"Email send failed: {e}")
```

---

# 4) Detect transitions and notify

After you build `df_all` (right after `df_all = pd.concat([...])`) **and before saving state**, insert:

```python
# Prepare SMTP config
smtp_cfg = {
    "server": args.smtp_server,
    "port": args.smtp_port,
    "user": args.smtp_user,
    "pass": args.smtp_pass,
    "from": args.email_from,
    "to": args.email_to,
    "ssl": bool(args.email_ssl),
    "prefix": args.email_subject_prefix
}

# Collect alerts only on NEW transitions into an alert status
alerts = []
for _, row in df_all.iterrows():
    key = f"{row['Strategy']}|{row['Ticker']}"
    cur_status = str(row["Status"])
    prev = state.get(key, {}).get("last_notified_status", "")

    if cur_status in ALERT_STATUSES and cur_status != prev:
        alerts.append(row)

# Send one email per run summarizing all triggered positions
if alerts and smtp_cfg["server"] and smtp_cfg["from"] and smtp_cfg["to"]:
    lines = []
    for r in alerts:
        lines.append(
            f"{r['Strategy']} {r['Ticker']} {r['Name']}\n"
            f"Status: {r['Status']}\n"
            f"Entry: {r['Entry']}  Price: {r['Price']}\n"
            f"StopPx: {r['StopPx']}  TakePx: {r['TakePx']}  TrailStopPx: {r['TrailStopPx']}\n"
            f"Qty: {r['Qty']}  Cost: ${r['Cost']}  Value: ${r['Value']}  PnL: ${r['PnL']} ({r['PnL%']}%)\n"
            "—" * 40
        )
    body = f"Triggered alerts ({len(alerts)}) at {dt.datetime.now().isoformat(timespec='seconds')}:\n\n" + "\n".join(lines)
    subj = f"{smtp_cfg['prefix']} {len(alerts)} alert(s): " + ", ".join({str(a['Ticker']) for a in alerts})
    send_email(smtp_cfg, subj, body)

    # Mark notified status
    for r in alerts:
        k = f"{r['Strategy']}|{r['Ticker']}"
        state[k]["last_notified_status"] = str(r["Status"])
```

Also, after printing tables (or right before saving state), make sure we **clear** `last_notified_status` if the position leaves an alert state (so you can be notified again on a new hit later). Add just before `save_json(args.state, state)`:

```python
for _, row in df_all.iterrows():
    k = f"{row['Strategy']}|{row['Ticker']}"
    if k in state and str(row["Status"]) not in ALERT_STATUSES:
        state[k]["last_notified_status"] = ""
```

---

# 5) Run examples

Gmail (use an **App Password**):

```bash
python tracker.py --positions positions.json --auto_activate_trailing \
  --smtp_server smtp.gmail.com --smtp_port 587 --smtp_user you@gmail.com \
  --smtp_pass "your-app-password" --email_from you@gmail.com \
  --email_to you@domain.com,ops@domain.com
```

SMTP SSL (port 465):

```bash
python tracker.py --positions positions.json --auto_activate_trailing \
  --smtp_server smtp.yourhost.com --smtp_port 465 --email_ssl \
  --smtp_user user@yourhost.com --smtp_pass "password" \
  --email_from alerts@yourhost.com --email_to you@domain.com
```

---

## Notes

* Alerts fire **once per transition** into an alert status; they won’t spam every run.
* If you want **per-position emails** (one message per alert), move the `send_email(...)` call into the loop.
* You can customize which statuses trigger by editing `ALERT_STATUSES`.

If you want Slack/Discord/webhook alerts too, I can add a `--webhook_url` and post JSON payloads the same way.
That depends on how “reactive” you want your stop-loss / take-profit / trailing-stop logic to be.

---

## **If you run it once per day**

* **Pros:**

  * Minimal noise
  * Less API calls / rate limit risk
  * Fewer email alerts
* **Cons:**

  * You may **miss intra-day moves**, especially for volatile high-risk holdings (TQQQ, crypto)
  * A price could hit your stop or trail level intraday and recover before you check — meaning you won’t catch it

---

## **If you run it more often (e.g., hourly during market hours)**

* **Pros:**

  * Catches hits as soon as they happen (much closer to live trading alerts)
  * Better suited for volatile instruments and trailing stops
* **Cons:**

  * More alerts, possibly more false positives if you react intraday
  * Higher CPU/network usage if running on a small host
  * Need to keep a process running (cron job, systemd service, or a small always-on server)

---

## **My recommendation for your setup**

Since you’ve got **high-risk positions** (leveraged ETFs, crypto) *and* trailing stops, I’d:

* **Run it every hour** during US market hours for stocks/ETFs.
* **Every 15–30 min** if you want close tracking of crypto (since it trades 24/7).

You can use `cron` or Windows Task Scheduler so it’s hands-off.

Example cron for hourly (Linux/macOS):

```bash
0 * * * * /path/to/python /path/to/tracker.py --positions positions.json --auto_activate_trailing --smtp_server smtp... --smtp_port ... --smtp_user ... --smtp_pass ... --email_from ... --email_to ...
```

---

If you want, I can show you **exact cron/TASKSCHED setup** so the tracker emails you automatically without you manually running it. That would make it a true “set and forget” alerting system.

Do you want me to set that up for you?
I can’t run that simulation here because `yfinance` isn’t installed in this environment, but you can run the same code locally in your Python setup.

If you paste that script into your system (with `pip install yfinance pandas numpy`), it will:

* Pull 10 years of daily prices for each ETF/asset.
* Calculate daily returns for each portfolio.
* Run 100,000 Monte Carlo simulations for a **6-month horizon**.
* Output **10th, 25th, 50th (median), 75th, 90th percentile** returns and mean return.

That way, you’ll see the **probability distribution** instead of just best/worst guesses.

Do you want me to rewrite it so it **only uses built-in Python** and no extra packages? That would run here without `yfinance`.
Perfect—here’s a ready-to-run Python script that uses **yfinance** to fetch 10 years of data, builds your two portfolios (High/Medium risk), runs a **Monte Carlo simulation** over a 6-month horizon (default 126 trading days), and prints percentile outcomes. It preserves **asset correlations** via a multivariate normal model on **daily log returns** with daily rebalancing.

### Install deps

```bash
pip install yfinance pandas numpy tabulate
```

### Save as `six_month_sim.py`

```python
import argparse
import datetime as dt
import numpy as np
import pandas as pd
import yfinance as yf
from tabulate import tabulate

# ------------ Default strategies (edit here or via CLI flags) ------------
DEFAULT_HIGH = {"TQQQ": 0.60, "EEM": 0.20, "ETH-USD": 0.20}
DEFAULT_MED  = {"SCHD": 0.40, "VOO": 0.40, "VCSH": 0.20}

# ------------ Data ------------
def download_adj_close(tickers, years=10):
    end = dt.date.today()
    start = end - dt.timedelta(days=int(365*years))
    df = yf.download(sorted(set(tickers)), start=start, end=end, progress=False)["Adj Close"]
    # Make it a DataFrame even when single ticker
    if isinstance(df, pd.Series):
        df = df.to_frame()
    return df.dropna(how="all")

def daily_log_returns(prices: pd.DataFrame) -> pd.DataFrame:
    return np.log(prices).diff().dropna(how="any")

# ------------ Simulation ------------
def simulate_portfolio(
    log_ret: pd.DataFrame,
    weights: dict,
    days: int = 126,
    sims: int = 100_000,
):
    # Align weights with columns
    cols = list(log_ret.columns)
    w = np.array([weights.get(c, 0.0) for c in cols], dtype=float)
    if not np.isclose(w.sum(), 1.0):
        raise ValueError(f"Weights must sum to 1. Got {w.sum():.4f}")

    mu = log_ret.mean().values        # mean vector of log returns
    cov = log_ret.cov().values        # covariance (captures correlation)

    # Generate (days x sims x assets) via multivariate normal per day
    # Efficient: draw (days*sims) vectors, then reshape
    rnd = np.random.default_rng()
    draws = rnd.multivariate_normal(mu, cov, size=(days, sims))  # shape (days, sims, assets)

    # Portfolio daily log returns (assumes daily rebalancing): r_p = w' * r_assets
    port_daily_log = np.tensordot(draws, w, axes=([2],[0]))  # shape (days, sims)

    # Cum 6-month return per simulation: exp(sum(log)) - 1
    cum_log = port_daily_log.sum(axis=0)        # shape (sims,)
    cum_ret = np.exp(cum_log) - 1.0
    return cum_ret

def describe(cum_returns: np.ndarray, initial: float = 1000.0):
    pct = [10, 25, 50, 75, 90]
    stats = {f"{p}th": np.percentile(cum_returns, p) for p in pct}
    stats["mean"] = float(np.mean(cum_returns))

    # Add dollar outcomes
    money = {k+"_value": initial * (1.0 + v) for k, v in stats.items()}
    # Format table rows
    rows = []
    for k in ["10th","25th","50th","75th","90th","mean"]:
        r = stats[k]
        rows.append([k, f"{r*100:,.2f}%", f"${money[k+'_value']:,.2f}"])
    return rows

# ------------ Utilities ------------
def parse_weights(s: str):
    """
    Parse weights like: "TQQQ:0.6,EEM:0.2,ETH-USD:0.2"
    """
    out = {}
    for part in s.split(","):
        t, v = part.split(":")
        out[t.strip()] = float(v)
    if not np.isclose(sum(out.values()), 1.0):
        raise ValueError("Weights must sum to 1.0")
    return out

def run_strategy(name: str, weights: dict, days: int, sims: int, initial: float):
    print(f"\n=== {name} ===")
    tickers = list(weights.keys())
    prices = download_adj_close(tickers, years=10)
    # Ensure all tickers exist in prices (yfinance may rename columns for single-ticker case)
    missing = set(tickers) - set(prices.columns)
    if missing:
        raise RuntimeError(f"Missing tickers in downloaded data: {missing}")

    logs = daily_log_returns(prices[tickers])
    cum = simulate_portfolio(logs, weights, days=days, sims=sims)
    table = describe(cum, initial=initial)
    print(tabulate(table, headers=["Percentile", "6M Return", "Final Value"], tablefmt="github"))
    return cum

# ------------ CLI ------------
def main():
    ap = argparse.ArgumentParser(description="6-Month Monte Carlo for Two Portfolios (correlation-aware)")
    ap.add_argument("--initial_high", type=float, default=1000.0, help="Initial dollars for High-Risk")
    ap.add_argument("--initial_med",  type=float, default=1000.0, help="Initial dollars for Medium-Risk")
    ap.add_argument("--days", type=int, default=126, help="Trading days to simulate (6 months ~ 126)")
    ap.add_argument("--sims", type=int, default=100000, help="Number of simulations")
    ap.add_argument("--high", type=str, default="", help='Override high-risk weights, e.g. "TQQQ:0.6,EEM:0.2,ETH-USD:0.2"')
    ap.add_argument("--med",  type=str, default="", help='Override medium-risk weights, e.g. "SCHD:0.4,VOO:0.4,VCSH:0.2"')
    ap.add_argument("--export_csv", action="store_true", help="Export simulation draws to CSV")
    args = ap.parse_args()

    high_w = parse_weights(args.high) if args.high else DEFAULT_HIGH
    med_w  = parse_weights(args.med)  if args.med  else DEFAULT_MED

    # High risk
    high_cum = run_strategy("High_Risk", high_w, days=args.days, sims=args.sims, initial=args.initial_high)
    # Medium risk
    med_cum = run_strategy("Medium_Risk", med_w, days=args.days, sims=args.sims, initial=args.initial_med)

    # Combined table
    combined = [
        ["High_Risk", f"Mean {np.mean(high_cum)*100:,.2f}%", f"${args.initial_high*(1+np.mean(high_cum)):,.2f}"],
        ["Medium_Risk", f"Mean {np.mean(med_cum)*100:,.2f}%", f"${args.initial_med*(1+np.mean(med_cum)):,.2f}"],
    ]
    print("\n=== Means (for reference) ===")
    print(tabulate(combined, headers=["Portfolio","Avg 6M Return","Avg Final Value"], tablefmt="github"))

    if args.export_csv:
        ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        pd.DataFrame({"High_Risk": high_cum}).to_csv(f"high_risk_sims_{ts}.csv", index=False)
        pd.DataFrame({"Medium_Risk": med_cum}).to_csv(f"medium_risk_sims_{ts}.csv", index=False)
        print(f"\nSaved CSVs: high_risk_sims_{ts}.csv, medium_risk_sims_{ts}.csv")

if __name__ == "__main__":
    main()
```

### Run it

Basic:

```bash
python six_month_sim.py
```

Custom sims/days:

```bash
python six_month_sim.py --sims 200000 --days 126
```

Override weights (must sum to 1.0):

```bash
python six_month_sim.py --high "TQQQ:0.55,EEM:0.25,ETH-USD:0.20" --med "SCHD:0.5,VOO:0.3,VCSH:0.2"
```

Export full simulation distributions to CSV:

```bash
python six_month_sim.py --export_csv
```

Want me to add an alternative **bootstrap** engine (sampling actual historical returns instead of normal) so you can compare parametric vs historical fat-tails?
Here’s a clean, no-nonsense checklist to get the 6-month Monte Carlo **simulation** running on your server(s).

# 1) Prep a clean Python env

Linux/macOS:

```bash
python3 -m venv venv
source venv/bin/activate
```

Windows (PowerShell):

```powershell
python -m venv venv
.\venv\Scripts\Activate.ps1
```

# 2) Install dependencies

```bash
pip install --upgrade pip
pip install yfinance pandas numpy tabulate
```

# 3) Save the script

Create a file named `six_month_sim.py` and paste in the code I sent earlier (the one that defines `DEFAULT_HIGH`, `DEFAULT_MED`, downloads 10y of data, runs correlation-aware Monte Carlo, and prints percentiles).

# 4) Quick sanity run

```bash
python six_month_sim.py
```

You should see two tables, one for **High\_Risk** and one for **Medium\_Risk**, with **10th/25th/50th/75th/90th/mean** 6-month returns and final values (assuming \$1,000 each).

# 5) Interpret results

* **50th (median)** ≈ “typical” outcome.
* **10th** = bad-case bound (≈1-in-10 worse).
* **90th** = optimistic bound (≈1-in-10 better).
  These are **portfolio** outcomes that respect historical **correlations** among the tickers.

# 6) Customize weights (must sum to 1.0)

```bash
python six_month_sim.py \
  --high "TQQQ:0.6,EEM:0.2,ETH-USD:0.2" \
  --med  "SCHD:0.4,VOO:0.4,VCSH:0.2"
```

# 7) Control horizon & accuracy

* Trading days for \~6 months: **126** (default).
* Simulations: start at **100,000** (default); bump if you want tighter percentiles.

```bash
python six_month_sim.py --days 126 --sims 200000
```

# 8) Export distributions for analysis

```bash
python six_month_sim.py --export_csv
```

This writes `high_risk_sims_<timestamp>.csv` and `medium_risk_sims_<timestamp>.csv` with one column of simulated 6-month returns. Load into pandas/Excel for charting.

# 9) (Optional) Pin versions for reproducibility

```bash
pip freeze > requirements.txt
```

Later:

```bash
pip install -r requirements.txt
```

# 10) (Optional) Automate runs

If you want periodic re-estimation (e.g., monthly) to reflect new history:

**Linux cron (monthly at 7:00):**

```bash
crontab -e
0 7 1 * * /full/path/venv/bin/python /full/path/six_month_sim.py --sims 200000 >> /full/path/sim.log 2>&1
```

**Windows Task Scheduler:**

* Action: `Program/script`: `C:\Path\to\python.exe`
* Arguments: `C:\Path\to\six_month_sim.py --sims 200000`
* Start in: your project folder

---

Want me to add a **bootstrap (historical resampling) engine** alongside the normal model so you can compare fat-tailed outcomes? Or wire the sim output straight into your existing **tracker** for scenario testing?


Alright — here’s a **step-by-step guide** to go from **zero to running your live tracker with email alerts**, so you can actively monitor and trade your two \$1,000 strategies.

---

## **1️⃣ Prep your trading accounts**

* Open brokerage accounts that allow:

  * **Stocks/ETFs** (for TQQQ, SCHD, etc.)
  * **Crypto** (for ETH-USD) — could be on Coinbase, Kraken, or your broker if they offer crypto.
* Fund each account with **\$1,000** for the two strategies, or \$2,000 total.
* Be sure you understand how to place:

  * Market orders
  * Stop-loss orders
  * Limit sell orders
  * Good-Til-Cancel (GTC) orders

---

## **2️⃣ Prepare the Python tracker**

You already have the **tracker.py** code from earlier that:

* Reads `positions.json` (allocations or quantities)
* Tracks live prices
* Detects `STOP`, `TAKE`, `TRAIL STOP`
* Sends **email alerts** when triggers occur

---

## **3️⃣ Create `positions.json`**

Example:

```json
{
  "High_Risk": [
    {"name": "Leveraged Nasdaq", "ticker": "TQQQ", "entry_price": 50.00, "allocation_usd": 600, "stop_loss_pct": -0.15, "take_profit_pct": 0.40, "trail_pct": 0.10},
    {"name": "Emerging Mkts", "ticker": "EEM", "entry_price": 40.00, "allocation_usd": 200, "stop_loss_pct": -0.12, "take_profit_pct": 0.25},
    {"name": "Ethereum", "ticker": "ETH-USD", "entry_price": 3200.00, "allocation_usd": 200, "stop_loss_pct": -0.20, "take_profit_pct": 0.50, "trail_pct": 0.12}
  ],
  "Medium_Risk": [
    {"name": "Dividend ETF", "ticker": "SCHD", "entry_price": 75.00, "allocation_usd": 400, "stop_loss_pct": -0.10, "take_profit_pct": 0.15},
    {"name": "S&P 500 ETF", "ticker": "VOO", "entry_price": 500.00, "allocation_usd": 400, "stop_loss_pct": -0.10, "take_profit_pct": 0.15},
    {"name": "ST Corp Bonds", "ticker": "VCSH", "entry_price": 80.00, "allocation_usd": 200}
  ]
}
```

💡 Use **actual fill prices** after you place the initial trades.

---

## **4️⃣ Set up email sending**

### Gmail (most common)

1. Enable **2FA** on your Gmail account.
2. Go to Google Account → Security → **App Passwords**.
3. Create a new app password for "Mail".
4. Copy the 16-character password — this is what you’ll use in `--smtp_pass`.

---

## **5️⃣ First run test**

From your tracker’s folder:

```bash
pip install yfinance pandas tabulate
python tracker.py --positions positions.json --auto_activate_trailing \
  --smtp_server smtp.gmail.com --smtp_port 587 \
  --smtp_user yourname@gmail.com --smtp_pass "your-app-password" \
  --email_from yourname@gmail.com --email_to you@domain.com
```

If configured correctly, you should:

* See the High\_Risk / Medium\_Risk tables
* Receive an email if any positions already meet a trigger condition

---

## **6️⃣ Place your trades**

* Log into your broker(s).
* Place **buy orders** for each position.
* Apply **stop-loss and take-profit** orders as per your JSON config (if your broker supports bracket orders).
* For trailing stops: the tracker will email you to manually adjust stops if needed.

---

## **7️⃣ Automate tracker runs**

For hourly checks during market hours:

**Linux/macOS** (`crontab -e`):

```bash
0 * * * 1-5 /path/to/python /path/to/tracker.py --positions /path/to/positions.json \
--auto_activate_trailing --smtp_server smtp.gmail.com --smtp_port 587 \
--smtp_user you@gmail.com --smtp_pass "app-password" \
--email_from you@gmail.com --email_to you@domain.com >> /path/to/tracker.log 2>&1
```

**Windows Task Scheduler**:

* Action: `Program/script`: `C:\Path\to\python.exe`
* Arguments: `C:\Path\to\tracker.py --positions C:\Path\positions.json --auto_activate_trailing --smtp_server smtp.gmail.com --smtp_port 587 --smtp_user you@gmail.com --smtp_pass app-password --email_from you@gmail.com --email_to you@domain.com`
* Trigger: Every hour, 9 AM to 4 PM ET, Mon–Fri

---

## **8️⃣ Daily routine**

* **Morning**: Check email + tracker logs to see if any alerts have fired overnight.
* **During market hours**: If you get an alert, log into your broker and:

  * Sell the position (if STOP hit)
  * Trim position / set trailing stop (if TAKE hit)
* **Crypto positions**: Monitor more often — crypto trades 24/7.

---

## **9️⃣ Weekly adjustments**

* Update `entry_price` in `positions.json` if you open new positions.
* Adjust `stop_loss_pct` or `take_profit_pct` if you change your trade plan.
* Periodically check your email settings by forcing a fake alert (temporarily set `take_profit_pct` very low).

---

## **🔟 Risk notes**

* Tracker **alerts** do not auto-trade — you must place orders manually unless you integrate with your broker’s API.
* Always verify price data before acting.
* In volatile markets, prices can gap past stop levels.

---

If you want, I can extend the tracker to **execute trades automatically** using Alpaca, Interactive Brokers, or TD Ameritrade APIs so alerts turn into real orders.
Do you want me to design that auto-trading flow next?

Alright — here’s the plan for turning your **tracker + alerts** into a **fully automated trader** so your two \$1,000 strategies can place orders without you clicking anything.

---

## **1️⃣ Choose a broker API**

You need a broker that:

* Supports **Python trading API**
* Allows **both stocks/ETFs and crypto** (or we use two APIs)

**Best options:**

* **Alpaca** (free, paper + live trading, supports US stocks/ETFs, crypto via API)
* **Interactive Brokers (IBKR)** (global access, more complex setup)
* TD Ameritrade, E\*TRADE, Robinhood — possible but API access is limited compared to Alpaca/IBKR.

For speed, **Alpaca** is the simplest to get going.
1fca74d2-3703-476c-aa56-0efbcd1424ed

---

## **2️⃣ Create API keys**

If using Alpaca:

1. Create account at [https://alpaca.markets](https://alpaca.markets)
2. Switch to **Live** or **Paper** mode (start with Paper!).
3. Get:

   * **API Key ID**
   * **API Secret Key**
   * API endpoint:

     * Paper: `https://paper-api.alpaca.markets`
     * Live: `https://api.alpaca.markets`

---

https://paper-api.alpaca.markets/v2
PK42ICUJSYD80MVDZ1KR
secret
3dBbSvRZDLv9JxdpNKhl4OrI3V36luOWLjk8Aiyv



## **3️⃣ Install SDK**


 curl -H "APCA-API-KEY-ID: PK42ICUJSYD80MVDZ1KR"  -H "APCA-API-SECRET-KEY:3dBbSvRZDLv9JxdpNKhl4OrI3V36luOWLjk8Aiyv"  https://paper-api.alpaca.markets/
v2/account

{"id":"46a1423c-f5eb-4470-954c-878bd49ffcfa","admin_configurations":{},"user_configurations":null,
"account_number":"PA3IZCAO05JG","status":"ACTIVE","crypto_status":"ACTIVE","options_approved_level":3,
"options_trading_level":3,"currency":"USD","buying_power":"40001","regt_buying_power":"40001",
"daytrading_buying_power":"0","effective_buying_power":"40001","non_marginable_buying_power":"18000.5",
"options_buying_power":"20000.5","bod_dtbp":"0","cash":"100000","accrued_fees":"0",
"portfolio_value":"100000","pattern_day_trader":false,"trading_blocked":false,
"transfers_blocked":false,"account_blocked":false,"created_at":"2025-08-09T15:20:32.301385Z",
"trade_suspended_by_user":false,"multiplier":"2","shorting_enabled":true,"equity":"100000",
"last_equity":"100000","long_market_value":"0","short_market_value":"0",
"position_market_value":"0","initial_margin":"79999.5","maintenance_margin":"0",
"last_maintenance_margin":"0","sma":"0","daytrade_count":0,"balance_asof":"2025-08-08",
"crypto_tier":0,"intraday_adjustments":"0","pending_reg_taf_fees":"0"}


```bash
pip install alpaca-trade-api
```

---

## **4️⃣ Modify tracker for auto-trading**

You’ll add a function to place trades when the status changes to `"Hit STOP"`, `"Hit TAKE"`, or `"Hit TRAIL STOP"`.

Example snippet:

```python
from alpaca_trade_api.rest import REST

# Config
ALPACA_KEY = "your-api-key"
ALPACA_SECRET = "your-api-secret"
ALPACA_BASE_URL = "https://paper-api.alpaca.markets"

api = REST(ALPACA_KEY, ALPACA_SECRET, ALPACA_BASE_URL)

def place_order(symbol, qty, side, order_type="market", time_in_force="gtc"):
    try:
        order = api.submit_order(
            symbol=symbol,
            qty=qty,
            side=side,  # 'buy' or 'sell'
            type=order_type,
            time_in_force=time_in_force
        )
        print(f"Order placed: {order}")
    except Exception as e:
        print(f"Order failed for {symbol}: {e}")
```

---

## **5️⃣ Hook into alert logic**

When your tracker detects a transition into an alert state, add:

```python
if cur_status == "Hit STOP":
    place_order(row["Ticker"], row["Qty"], "sell")
elif cur_status == "Hit TAKE":
    # For TAKE, maybe sell half
    place_order(row["Ticker"], round(row["Qty"]/2, 6), "sell")
elif cur_status == "Hit TRAIL STOP":
    place_order(row["Ticker"], row["Qty"], "sell")
```

---

## **6️⃣ For crypto**

* Alpaca supports `BTC/USD`, `ETH/USD` via same API — so your `Ticker` might need mapping:

```python
def map_ticker(ticker):
    if ticker == "ETH-USD":
        return "ETH/USD"
    return ticker
```

---

## **7️⃣ Safety first**

Before going live:

1. Use **Paper mode** for at least 1–2 weeks.
2. Log every trade to a CSV so you can reconcile later.
3. Keep **email alerts enabled** so you know when the bot trades.

---

## **8️⃣ Automation**

* Same as before: use `cron` (Linux) or Task Scheduler (Windows) to run your tracker hourly or every 15 min for crypto.
* In live mode, this means trades can execute automatically without you being at the screen.

---

## **9️⃣ What you’ll end up with**

* Tracker reads `positions.json`
* Fetches prices → detects triggers
* Sends you an email
* Submits sell orders via API automatically
* (Optional) Submits **buy orders** if you have re-entry rules

---

I can give you a **full tracker.py version** that:

* Combines price tracking, trailing stop logic, email alerts
* Integrates Alpaca API calls for stocks, ETFs, and ETH/USD
* Logs trades to a `trades_log.csv`

Do you want me to prepare **that complete integrated script** so you can drop in your API keys and run it? That would replace your current tracker entirely.
You got it. Here’s a **single, integrated Python script** that:

* Tracks your two strategies (High/Medium)
* Detects **STOP / TAKE / TRAIL STOP** with persistent state
* **Emails** you on new triggers
* **Auto-trades** via **Alpaca** (paper or live), including **fractional**/**notional** orders
* Can **rebalance** to target allocations using your **available cash or equity-scaled budgets**
* Logs trades to `trades_log.csv`

Save this as `autotrader.py`.

---

```python
#!/usr/bin/env python3
"""
autotrader.py
Two-strategy tracker (High/Medium) with:
- yfinance price feed
- STOP / TAKE / TRAIL STOP logic (persistent)
- Email alerts
- Alpaca auto-trading (paper/live)
- Optional portfolio rebalance to target allocations (fractional/notional)
- Trade logging

Phil-ready: drop in your Alpaca API keys & SMTP creds via CLI or env and run.
"""

import argparse
import datetime as dt
import json
import os
import sys
import time
from typing import Dict, List, Any, Tuple

import numpy as np
import pandas as pd
import yfinance as yf
from tabulate import tabulate

# -------- Email ----------
import smtplib
from email.mime.text import MIMEText
from email.utils import formatdate

ALERT_STATUSES = {"Hit STOP", "Hit TAKE", "Hit TRAIL STOP"}

# -------- Alpaca ----------
try:
    from alpaca_trade_api.rest import REST, TimeFrame
    HAS_ALPACA = True
except Exception:
    HAS_ALPACA = False


# ===================== Utils: JSON I/O =====================
def load_json(path: str) -> Any:
    with open(path, "r") as f:
        return json.load(f)

def save_json(path: str, data: Any) -> None:
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, indent=2)
    os.replace(tmp, path)


# ===================== Positions & State =====================
"""
positions.json structure (example):

{
  "High_Risk": [
    { "name": "Leveraged Nasdaq", "ticker": "TQQQ", "entry_price": 50.0,
      "target_weight": 0.60, "stop_loss_pct": -0.15, "take_profit_pct": 0.40, "trail_pct": 0.10 },
    { "name": "Emerging Mkts", "ticker": "EEM", "entry_price": 40.0,
      "target_weight": 0.20, "stop_loss_pct": -0.12, "take_profit_pct": 0.25 },
    { "name": "Ethereum", "ticker": "ETH-USD", "entry_price": 3200.0,
      "target_weight": 0.20, "stop_loss_pct": -0.20, "take_profit_pct": 0.50, "trail_pct": 0.12 }
  ],
  "Medium_Risk": [
    { "name": "Dividend ETF", "ticker": "SCHD", "entry_price": 75.0,
      "target_weight": 0.40, "stop_loss_pct": -0.10, "take_profit_pct": 0.15 },
    { "name": "S&P 500 ETF", "ticker": "VOO", "entry_price": 500.0,
      "target_weight": 0.40, "stop_loss_pct": -0.10, "take_profit_pct": 0.15 },
    { "name": "ST Corp Bonds", "ticker": "VCSH", "entry_price": 80.0,
      "target_weight": 0.20 }
  ]
}
"""

def load_positions(path: str) -> Dict[str, List[dict]]:
    data = load_json(path)
    if not isinstance(data, dict) or "High_Risk" not in data or "Medium_Risk" not in data:
        raise ValueError("positions.json must contain 'High_Risk' and 'Medium_Risk' arrays.")
    # Basic weight sanity
    for strat in ("High_Risk", "Medium_Risk"):
        s = data[strat]
        if any("target_weight" not in p for p in s):
            raise ValueError(f"Every position in {strat} needs 'target_weight'.")
        ws = sum(float(p["target_weight"]) for p in s)
        if abs(ws - 1.0) > 1e-6:
            raise ValueError(f"Weights in {strat} must sum to 1.0 (got {ws}).")
    return data

def ensure_state(path: str, positions: Dict[str, List[dict]]) -> Dict[str, dict]:
    """
    State per strategy|ticker:
      trailing_active: bool
      trail_pct: float
      peak_price: float or None
      take_trigger_price: float or None
      last_notified_status: str
    """
    if os.path.exists(path):
        state = load_json(path)
    else:
        state = {}

    for strat in ("High_Risk", "Medium_Risk"):
        for p in positions[strat]:
            key = f"{strat}|{p['ticker']}"
            st = state.setdefault(key, {})
            st.setdefault("trailing_active", False)
            st["trail_pct"] = float(p.get("trail_pct", st.get("trail_pct", 0.10)))
            st.setdefault("peak_price", None)
            st.setdefault("take_trigger_price", None)
            st.setdefault("last_notified_status", "")
    return state


# ===================== Prices =====================
def fetch_prices(tickers: List[str]) -> Dict[str, float]:
    """
    Pull latest prices via yfinance. Falls back gracefully.
    """
    unique = sorted(set(tickers))
    prices: Dict[str, float] = {}
    try:
        data = yf.download(unique, period="1d", interval="1m", progress=False, threads=True)
        if "Close" in data:
            close = data["Close"]
            if isinstance(close, pd.Series):
                last = close.dropna().iloc[-1]
                prices[unique[0]] = float(last)
            else:
                last_row = close.dropna(how="all").iloc[-1]
                for t in unique:
                    val = last_row.get(t)
                    if pd.isna(val):
                        val = yf.Ticker(t).fast_info.last_price
                    prices[t] = float(val)
        else:
            for t in unique:
                prices[t] = float(yf.Ticker(t).fast_info.last_price)
    except Exception:
        # Full fallback
        for t in unique:
            prices[t] = float(yf.Ticker(t).fast_info.last_price)
    return prices


# ===================== Email =====================
def send_email(smtp_cfg: dict, subject: str, body: str):
    if not smtp_cfg.get("server") or not smtp_cfg.get("from") or not smtp_cfg.get("to"):
        print("Email not configured; skipping.")
        return
    msg = MIMEText(body, "plain", "utf-8")
    msg["Subject"] = subject
    msg["From"] = smtp_cfg["from"]
    msg["To"] = smtp_cfg["to"]
    msg["Date"] = formatdate(localtime=True)

    try:
        if smtp_cfg.get("ssl"):
            with smtplib.SMTP_SSL(smtp_cfg["server"], smtp_cfg["port"]) as s:
                if smtp_cfg.get("user"):
                    s.login(smtp_cfg["user"], smtp_cfg["pass"])
                s.send_message(msg)
        else:
            with smtplib.SMTP(smtp_cfg["server"], smtp_cfg["port"]) as s:
                s.ehlo()
                s.starttls()
                s.ehlo()
                if smtp_cfg.get("user"):
                    s.login(smtp_cfg["user"], smtp_cfg["pass"])
                s.send_message(msg)
        print("Alert email sent.")
    except Exception as e:
        print(f"Email send failed: {e}")


# ===================== Alpaca wrapper =====================
class AlpacaClient:
    def __init__(self, key: str, secret: str, base_url: str, paper: bool = True):
        if not HAS_ALPACA:
            raise RuntimeError("alpaca-trade-api not installed: pip install alpaca-trade-api")
        self.api = REST(key, secret, base_url)
        self.paper = paper

    @staticmethod
    def map_ticker_to_alpaca(symbol: str) -> str:
        # Alpaca uses "ETH/USD" for crypto; equities are same (e.g., "VOO")
        if symbol.upper() == "ETH-USD":
            return "ETH/USD"
        return symbol.upper()

    def get_account_equity_cash(self) -> Tuple[float, float]:
        acct = self.api.get_account()
        # Equity includes positions; cash is buying power baseline
        return float(acct.equity), float(acct.cash)

    def get_position_qty_value(self, symbol: str) -> Tuple[float, float]:
        sym = self.map_ticker_to_alpaca(symbol)
        try:
            p = self.api.get_position(sym)
            qty = float(p.qty) if hasattr(p, "qty") else float(p.qty_available)
            # For equities qty is shares; for crypto qty is units
            # Market value provided:
            val = float(p.market_value)
            return qty, val
        except Exception:
            return 0.0, 0.0

    def submit_notional_order(self, symbol: str, notional_usd: float, side: str, tif: str = "gtc"):
        sym = self.map_ticker_to_alpaca(symbol)
        notional = round(float(notional_usd), 2)
        if notional <= 0:
            return None
        try:
            order = self.api.submit_order(symbol=sym, notional=notional, side=side, type="market", time_in_force=tif)
            print(f"[ALPACA] {side.upper()} ${notional} of {sym} → {order.id}")
            return order
        except Exception as e:
            print(f"[ALPACA] Notional order failed for {sym}: {e}")
            return None

    def submit_qty_order(self, symbol: str, qty: float, side: str, tif: str = "gtc"):
        sym = self.map_ticker_to_alpaca(symbol)
        q = float(qty)
        if q <= 0:
            return None
        try:
            order = self.api.submit_order(symbol=sym, qty=q, side=side, type="market", time_in_force=tif)
            print(f"[ALPACA] {side.upper()} {q} of {sym} → {order.id}")
            return order
        except Exception as e:
            print(f"[ALPACA] Qty order failed for {sym}: {e}")
            return None


# ===================== Core evaluation =====================
def evaluate_positions(
    strategy_name: str,
    positions: List[dict],
    live_prices: Dict[str, float],
    state: Dict[str, dict],
    auto_trail: bool
) -> Tuple[pd.DataFrame, Dict[str, dict]]:
    rows = []
    now = dt.datetime.now().isoformat(timespec="seconds")
    for p in positions:
        ticker = p["ticker"]
        entry = float(p["entry_price"])
        price = float(live_prices.get(ticker, entry))
        stop_loss_pct = p.get("stop_loss_pct", None)
        take_profit_pct = p.get("take_profit_pct", None)

        stop_px = entry * (1.0 + stop_loss_pct) if isinstance(stop_loss_pct, (int, float)) else None
        tp_px = entry * (1.0 + take_profit_pct) if isinstance(take_profit_pct, (int, float)) else None

        key = f"{strategy_name}|{ticker}"
        s = state[key]
        trailing_active = bool(s.get("trailing_active", False))
        trail_pct = float(s.get("trail_pct", p.get("trail_pct", 0.10)))
        peak_price = s.get("peak_price", None)
        take_trigger_price = s.get("take_trigger_price", None)
        status = "Active"
        trail_stop_px = ""

        # Hard stop (before trailing)
        if not trailing_active and stop_px is not None and price <= stop_px:
            status = "Hit STOP"

        # Take-profit activation
        if take_profit_pct is not None and price >= tp_px:
            if auto_trail and not trailing_active:
                trailing_active = True
                peak_price = price
                take_trigger_price = tp_px
                status = "Trailing"
            elif not auto_trail and not trailing_active:
                status = "Hit TAKE (not trailing)"
            else:
                status = "Trailing"

        # Trailing logic
        if trailing_active:
            if peak_price is None or price > peak_price:
                peak_price = price
            trail_stop = peak_price * (1.0 - trail_pct)
            trail_stop_px = round(trail_stop, 6)
            if price <= trail_stop:
                status = "Hit TRAIL STOP"
            elif "Trailing" not in status and "TAKE" not in status:
                status = "Trailing"

        # Persist
        s["trailing_active"] = trailing_active
        s["trail_pct"] = trail_pct
        s["peak_price"] = peak_price
        s["take_trigger_price"] = take_trigger_price

        pnl_pct = (price / entry - 1.0) if entry > 0 else 0.0

        rows.append({
            "TS": now,
            "Strategy": strategy_name,
            "Name": p.get("name", ""),
            "Ticker": ticker,
            "Entry": round(entry, 6),
            "Price": round(price, 6),
            "StopPx": (round(stop_px, 6) if stop_px is not None else ""),
            "TakePx": (round(tp_px, 6) if tp_px is not None else ""),
            "Trail%": round(trail_pct * 100.0, 2) if trailing_active or "trail_pct" in p else "",
            "TrailStopPx": trail_stop_px,
            "PnL%": round(pnl_pct * 100.0, 2),
            "Status": status
        })

    return pd.DataFrame(rows), state


# ===================== Rebalance =====================
def rebalance_to_targets(
    api: AlpacaClient,
    strategy_name: str,
    positions: List[dict],
    live_prices: Dict[str, float],
    budget_usd: float,
    min_trade_usd: float = 20.0,
    tolerance_usd: float = 10.0
):
    """
    Move current market values toward target allocations within 'budget_usd'.
    Uses NOTIONAL market orders (fractional).
    """
    if budget_usd <= 0:
        return []

    # Current values per ticker from Alpaca positions (best) or estimate from price*qty
    actions = []
    # Compute targets
    targets = {p["ticker"]: float(p["target_weight"]) * budget_usd for p in positions}

    # Current values:
    current_values = {}
    for p in positions:
        t = p["ticker"]
        _, val = api.get_position_qty_value(t)
        if val == 0.0:
            # approximate with zero if no position
            current_values[t] = 0.0
        else:
            current_values[t] = val

    # Deltas
    for t, tgt in targets.items():
        cur = current_values.get(t, 0.0)
        diff = tgt - cur  # positive => need to BUY
        if abs(diff) >= max(min_trade_usd, tolerance_usd):
            side = "buy" if diff > 0 else "sell"
            notional = abs(diff)
            actions.append((t, side, notional))

    # Place notional orders
    placed = []
    for t, side, notional in actions:
        order = api.submit_notional_order(t, notional, side)
        if order:
            placed.append((t, side, round(notional, 2), order.id))
    return placed


# ===================== Trade logging =====================
def append_trade_log(path: str, rows: List[dict]):
    if not rows:
        return
    cols = ["TS", "Strategy", "Ticker", "Action", "Qty", "NotionalUSD", "OrderId", "Reason"]
    df_new = pd.DataFrame(rows, columns=cols)
    if os.path.exists(path):
        df_old = pd.read_csv(path)
        df_all = pd.concat([df_old, df_new], ignore_index=True)
    else:
        df_all = df_new
    df_all.to_csv(path, index=False)


# ===================== Main =====================
def main():
    ap = argparse.ArgumentParser(description="Two-Strategy AutoTrader (tracker + email + Alpaca + rebalance)")

    ap.add_argument("--positions", default="positions.json")
    ap.add_argument("--state", default="tracker_state.json")
    ap.add_argument("--trades_log", default="trades_log.csv")

    # Alerts / trailing
    ap.add_argument("--auto_activate_trailing", action="store_true")

    # SMTPtfa
    ap.add_argument("--smtp_server", default=os.getenv("SMTP_SERVER", ""))
    ap.add_argument("--smtp_port", type=int, default=int(os.getenv("SMTP_PORT", "587")))
    ap.add_argument("--smtp_user", default=os.getenv("SMTP_USER", ""))
    ap.add_argument("--smtp_pass", default=os.getenv("SMTP_PASS", ""))
    ap.add_argument("--email_from", default=os.getenv("EMAIL_FROM", ""))
    ap.add_argument("--email_to", default=os.getenv("EMAIL_TO", ""))
    ap.add_argument("--email_ssl", action="store_true")
    ap.add_argument("--email_subject_prefix", default="[AutoTrader]")

    # Alpaca
    ap.add_argument("--alpaca_key", default=os.getenv("ALPACA_KEY_ID", ""))
    ap.add_argument("--alpaca_secret", default=os.getenv("ALPACA_SECRET_KEY", ""))
    ap.add_argument("--alpaca_base_url", default=os.getenv("ALPACA_BASE_URL", "https://paper-api.alpaca.markets"))

    # Budgets (scaling)
    ap.add_argument("--budget_high", type=float, default=1000.0, help="USD budget for High_Risk targets")
    ap.add_argument("--budget_med", type=float, default=1000.0, help="USD budget for Medium_Risk targets")
    ap.add_argument("--scale_to_equity", action="store_true",
                    help="Scale budgets to fraction of account equity (see --equity_frac_high/med)")
    ap.add_argument("--equity_frac_high", type=float, default=0.0,
                    help="If scale_to_equity, use equity * this fraction for High_Risk")
    ap.add_argument("--equity_frac_med", type=float, default=0.0,
                    help="If scale_to_equity, use equity * this fraction for Medium_Risk")

    # Rebalance control
    ap.add_argument("--rebalance", action="store_true", help="Perform rebalance toward targets")
    ap.add_argument("--min_trade_usd", type=float, default=20.0)
    ap.add_argument("--tolerance_usd", type=float, default=10.0)

    # Safety / run mode
    ap.add_argument("--dry_run", action="store_true", help="Do not place orders; just print actions")
    ap.add_argument("--force_reset_state", action="store_true")

    args = ap.parse_args()

    # Load positions
    positions = load_positions(args.positions)

    # State
    if args.force_reset_state and os.path.exists(args.state):
        os.remove(args.state)
    state = ensure_state(args.state, positions)

    # Prices
    tickers = [p["ticker"] for k in ("High_Risk", "Medium_Risk") for p in positions[k]]
    live_prices = fetch_prices(tickers)

    # Evaluate
    df_high, state = evaluate_positions("High_Risk", positions["High_Risk"], live_prices, state, args.auto_activate_trailing)
    df_med, state = evaluate_positions("Medium_Risk", positions["Medium_Risk"], live_prices, state, args.auto_activate_trailing)
    df_all = pd.concat([df_high, df_med], ignore_index=True)

    # Print tables
    if not df_high.empty:
        print("\n=== High_Risk ===")
        print(tabulate(df_high, headers="keys", tablefmt="github", showindex=False))
    if not df_med.empty:
        print("\n=== Medium_Risk ===")
        print(tabulate(df_med, headers="keys", tablefmt="github", showindex=False))

    # Email alerts on NEW transitions
    smtp_cfg = {
        "server": args.smtp_server, "port": args.smtp_port,
        "user": args.smtp_user, "pass": args.smtp_pass,
        "from": args.email_from, "to": args.email_to,
        "ssl": bool(args.email_ssl), "prefix": args.email_subject_prefix
    }
    alerts = []
    for _, row in df_all.iterrows():
        key = f"{row['Strategy']}|{row['Ticker']}"
        cur_status = str(row["Status"])
        prev = state.get(key, {}).get("last_notified_status", "")
        if cur_status in ALERT_STATUSES and cur_status != prev:
            alerts.append(row)

    if alerts and smtp_cfg["server"] and smtp_cfg["from"] and smtp_cfg["to"]:
        lines = []
        for r in alerts:
            lines.append(
                f"{r['Strategy']} {r['Ticker']} {r['Name']}\n"
                f"Status: {r['Status']}\n"
                f"Entry: {r['Entry']}  Price: {r['Price']}\n"
                f"StopPx: {r['StopPx']}  TakePx: {r['TakePx']}  TrailStopPx: {r['TrailStopPx']}\n"
                f"PnL%: {r['PnL%']}%\n"
                + "—"*40
            )
        body = f"Triggered alerts ({len(alerts)}) at {dt.datetime.now().isoformat(timespec='seconds')}:\n\n" + "\n".join(lines)
        subj = f"{smtp_cfg['prefix']} {len(alerts)} alert(s): " + ", ".join({str(a['Ticker']) for a in alerts})
        send_email(smtp_cfg, subj, body)

        # mark notified
        for r in alerts:
            k = f"{r['Strategy']}|{r['Ticker']}"
            state[k]["last_notified_status"] = str(r["Status"])

    # Clear notification mark if left alert states
    for _, row in df_all.iterrows():
        k = f"{row['Strategy']}|{row['Ticker']}"
        if k in state and str(row["Status"]) not in ALERT_STATUSES:
            state[k]["last_notified_status"] = ""

    # ====== Auto-trading on triggers ======
    trade_rows = []
    if HAS_ALPACA and (args.alpaca_key and args.alpaca_secret and args.alpaca_base_url):
        alp = AlpacaClient(args.alpaca_key, args.alpaca_secret, args.alpaca_base_url)
        # SELL logic on triggers
        for _, r in df_all.iterrows():
            status = str(r["Status"])
            if status not in ALERT_STATUSES:
                continue

            ticker = r["Ticker"]
            qty_have, val_have = alp.get_position_qty_value(ticker)
            if qty_have <= 0 and val_have <= 0:
                continue

            action = None
            order = None
            if status == "Hit STOP":
                action = ("sell_all", qty_have, 0.0, "STOP")
                if not args.dry_run:
                    order = alp.submit_qty_order(ticker, qty_have, "sell")
            elif status == "Hit TAKE":
                sell_qty = max(round(qty_have / 2, 6), 0.0)
                if sell_qty > 0:
                    action = ("sell_half", sell_qty, 0.0, "TAKE")
                    if not args.dry_run:
                        order = alp.submit_qty_order(ticker, sell_qty, "sell")
            elif status == "Hit TRAIL STOP":
                action = ("sell_all", qty_have, 0.0, "TRAIL_STOP")
                if not args.dry_run:
                    order = alp.submit_qty_order(ticker, qty_have, "sell")

            if action:
                trade_rows.append({
                    "TS": r["TS"],
                    "Strategy": r["Strategy"],
                    "Ticker": ticker,
                    "Action": action[0],
                    "Qty": action[1],
                    "NotionalUSD": action[2],
                    "OrderId": getattr(order, "id", "") if order else "",
                    "Reason": action[3]
                })

        # Optional: rebalance to targets using budgets (scaled to equity if requested)
        if args.rebalance:
            equity, cash = alp.get_account_equity_cash()
            budget_high = args.budget_high
            budget_med = args.budget_med
            if args.scale_to_equity:
                if args.equity_frac_high > 0:
                    budget_high = equity * args.equity_frac_high
                if args.equity_frac_med > 0:
                    budget_med = equity * args.equity_frac_med

            # Place notional orders to move toward targets
            for strat, budget in (("High_Risk", budget_high), ("Medium_Risk", budget_med)):
                placed = []
                if not args.dry_run:
                    placed = rebalance_to_targets(
                        alp, strat, positions[strat], live_prices,
                        budget_usd=budget,
                        min_trade_usd=args.min_trade_usd,
                        tolerance_usd=args.tolerance_usd
                    )
                # Log
                ts_now = dt.datetime.now().isoformat(timespec="seconds")
                for (t, side, notional, oid) in placed:
                    trade_rows.append({
                        "TS": ts_now,
                        "Strategy": strat,
                        "Ticker": t,
                        "Action": f"{side}_notional",
                        "Qty": "",
                        "NotionalUSD": notional,
                        "OrderId": oid,
                        "Reason": "rebalance"
                    })

    else:
        print("Alpaca not configured; skipping auto-trading. (Use --alpaca_key/--alpaca_secret/--alpaca_base_url)")

    # Save trade logs and state
    append_trade_log(args.trades_log, trade_rows)
    save_json(args.state, state)

    # Summary
    summary = df_all.groupby("Strategy", as_index=False).agg(
        Count=("Ticker","count"),
    )
    print("\n=== Run Summary ===")
    print(tabulate(summary, headers="keys", tablefmt="github", showindex=False))
    if trade_rows:
        print(f"\nLogged {len(trade_rows)} trade(s) to {args.trades_log}.")


if __name__ == "__main__":
    main()
```

---

### Minimal `positions.json` example

```json
{
  "High_Risk": [
    {"name": "Leveraged Nasdaq", "ticker": "TQQQ", "entry_price": 50.00, "target_weight": 0.60, "stop_loss_pct": -0.15, "take_profit_pct": 0.40, "trail_pct": 0.10},
    {"name": "Emerging Mkts", "ticker": "EEM", "entry_price": 40.00, "target_weight": 0.20, "stop_loss_pct": -0.12, "take_profit_pct": 0.25},
    {"name": "Ethereum", "ticker": "ETH-USD", "entry_price": 3200.00, "target_weight": 0.20, "stop_loss_pct": -0.20, "take_profit_pct": 0.50, "trail_pct": 0.12}
  ],
  "Medium_Risk": [
    {"name": "Dividend ETF", "ticker": "SCHD", "entry_price": 75.00, "target_weight": 0.40, "stop_loss_pct": -0.10, "take_profit_pct": 0.15},
    {"name": "S&P 500 ETF", "ticker": "VOO", "entry_price": 500.00, "target_weight": 0.40, "stop_loss_pct": -0.10, "take_profit_pct": 0.15},
    {"name": "ST Corp Bonds", "ticker": "VCSH", "entry_price": 80.00, "target_weight": 0.20}
  ]
}
```

---

### Install + run

```bash
pip install yfinance pandas tabulate alpaca-trade-api
```

Paper trading + hourly cron-friendly:

```bash
python autotrader.py --positions positions.json --auto_activate_trailing \
  --alpaca_key "$ALPACA_KEY_ID" \
  --alpaca_secret "$ALPACA_SECRET_KEY" \
  --alpaca_base_url "https://paper-api.alpaca.markets" \
  --smtp_server smtp.gmail.com --smtp_port 587 \
  --smtp_user you@gmail.com --smtp_pass "your-app-password" \
  --email_from you@gmail.com --email_to you@domain.com \
  --rebalance --budget_high 1000 --budget_med 1000
```

Scale budgets to equity (e.g., 10% and 10% of equity):

```bash
python autotrader.py --positions positions.json --auto_activate_trailing \
  --alpaca_key "$ALPACA_KEY_ID" --alpaca_secret "$ALPACA_SECRET_KEY" \
  --alpaca_base_url "https://paper-api.alpaca.markets" \
  --scale_to_equity --equity_frac_high 0.10 --equity_frac_med 0.10 \
  --rebalance
```

Dry run (no orders), just emails + prints:

```bash
python autotrader.py --positions positions.json --auto_activate_trailing --dry_run
```

---

### Notes and gotchas

* **Fractional/notional orders:** The script uses **notional USD** for rebalancing (works for Alpaca stocks and crypto; your account must be enabled for fractional). Triggered **sells** use **qty** to dump all/half precisely.
* **Market hours:** Equities will queue GTC if market is closed; crypto is 24/7.
* **Entry prices:** Use your actual fills. These only drive the alert logic (stops/takes/trailing), not order pricing (market orders).
* **State:** Trailing peak & last-notified status live in `tracker_state.json`. Use `--force_reset_state` to reset.
* **Logs:** Orders appended to `trades_log.csv`.

If you want this to also **auto-enter** new positions on entry signals (e.g., MA crossovers) instead of just managing stops/takes/rebalances, say the word and I’ll drop in a simple rule engine for entries too.
