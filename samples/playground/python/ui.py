"""Terminal UI helpers for Foundry Local Playground.

Handles all terminal drawing so the main sample can focus on SDK calls.
"""

import sys
import shutil
import re
import threading

BLOCK_FULL = "█"
BLOCK_EMPTY = "░"
BAR_WIDTH = 30
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")


# ── Primitives ────────────────────────────────────────────────────────────

def _cols():
    return shutil.get_terminal_size().columns


def _write(s):
    sys.stdout.write(s)
    sys.stdout.flush()


def _visible_len(s):
    return len(ANSI_RE.sub("", s))


def _pad_visible(s, width):
    return s + " " * max(0, width - _visible_len(s))


def _seg(w):
    return "─" * (w + 2)


def _table_hr(widths, pos):
    L, M, R = (
        ("┌", "┬", "┐") if pos == "top" else
        ("├", "┼", "┤") if pos == "mid" else
        ("└", "┴", "┘")
    )
    return "  " + L + M.join(_seg(w) for w in widths) + R


def _table_row(widths, values):
    cells = "".join(f" {_pad_visible(values[i] if i < len(values) else '', w)} │"
                    for i, w in enumerate(widths))
    return f"  │{cells}"


def progress_bar(percent, width=BAR_WIDTH):
    filled = int(width * percent / 100)
    return BLOCK_FULL * filled + BLOCK_EMPTY * (width - filled)


def section(title):
    c = _cols()
    print(f"\n{'─' * c}")
    print(f"  {title}")
    print("─" * c)


def wrap_text(text, max_width):
    result = []
    for para in text.split("\n"):
        if not para:
            result.append("")
            continue
        line = ""
        for word in para.split(" "):
            if line and len(line) + len(word) + 1 > max_width:
                result.append(line)
                line = word
            else:
                line = f"{line} {word}" if line else word
        if line:
            result.append(line)
    return result or [""]


# ── EP Table with live progress ───────────────────────────────────────────

def show_ep_table(eps):
    if not eps:
        print("  No execution providers found.")
        return {"on_progress": lambda *a: None, "finalize": lambda *a: None}

    COL1 = max(7, *(len(ep.name) for ep in eps))
    COL2 = BAR_WIDTH + 7
    W = [COL1, COL2]
    fmt = lambda name, cell: _table_row(W, [name, cell])

    print(_table_hr(W, "top"))
    print(fmt("EP Name", "Status"))
    print(_table_hr(W, "mid"))

    ep_idx = {ep.name: i for i, ep in enumerate(eps)}
    lock = threading.Lock()
    for ep in eps:
        cell = ("\x1b[32m● registered\x1b[0m" if ep.is_registered
                else progress_bar(0) + "  0.0%")
        print(fmt(ep.name, cell))
    print(_table_hr(W, "bot"))

    # Cursor sits one line below the table. Update only one row in place.
    def _update_row(idx, cell):
        up = len(eps) - idx + 1
        _write(f"\x1b[{up}A\r\x1b[K")
        _write(fmt(eps[idx].name, cell))
        _write(f"\x1b[K\x1b[{up}B\r")

    def on_progress(ep_name, percent):
        idx = ep_idx.get(ep_name)
        if idx is None:
            return
        with lock:
            _update_row(idx, progress_bar(percent) + f" {percent:5.1f}%")

    def finalize(result):
        failed_set = set(result.failed_eps or []) if result else set()
        with lock:
            for ep in eps:
                idx = ep_idx[ep.name]
                ok = ep.is_registered or ep.name not in failed_set
                dot = "\x1b[32m● registered\x1b[0m" if ok else "\x1b[31m● failed\x1b[0m"
                _update_row(idx, dot)

    return {"on_progress": on_progress, "finalize": finalize}


# ── Model Catalog Table ──────────────────────────────────────────────────

def show_catalog(models):
    rows = []
    for i, m in enumerate(models):
        for v_idx, v in enumerate(m.variants):
            rows.append({"model_idx": i, "variant_idx": v_idx, "model": m, "variant": v})

    MC = [
        max(2, len(str(len(rows)))),
        max(5, *(len(m.alias) for m in models)),
        max(7, *(len(r["variant"].id) for r in rows)),
        10,
        max(4, *(len(m.info.task or "?") for m in models)),
        6,
    ]

    print(_table_hr(MC, "top"))
    print(_table_row(MC, ["#", "Alias", "Variant", "Size (GB)", "Task", "Cached"]))
    print(_table_hr(MC, "mid"))

    num = 1
    for i, m in enumerate(models):
        if i > 0:
            print(_table_hr(MC, "mid"))
        size_gb = f"{m.info.file_size_mb / 1024:.1f}" if m.info.file_size_mb else "?"
        task = m.info.task or "?"
        for v_idx, v in enumerate(m.variants):
            dot = "\x1b[32m●\x1b[0m" if v.is_cached else "\x1b[31m●\x1b[0m"
            print(_table_row(MC, [
                str(num),
                m.alias if v_idx == 0 else "",
                v.id,
                size_gb if v_idx == 0 else "",
                task if v_idx == 0 else "",
                dot,
            ]))
            num += 1
    print(_table_hr(MC, "bot"))
    return rows


# ── Download Progress Bar ─────────────────────────────────────────────────

def create_download_bar(model_alias):
    COL1 = max(5, len(model_alias))
    COL2 = BAR_WIDTH + 7
    W = [COL1, COL2]
    fmt = lambda n, c: _table_row(W, [n, c])

    print(_table_hr(W, "top"))
    print(fmt("Model", "Progress"))
    print(_table_hr(W, "mid"))
    print(fmt(model_alias, progress_bar(0) + "  0.0%"))
    print(_table_hr(W, "bot"))

    def on_progress(percent):
        _write("\x1b[2A\r")
        _write(fmt(model_alias, progress_bar(percent) + f" {percent:5.1f}%"))
        _write("\x1b[K\n\x1b[1B")

    def finalize():
        _write("\x1b[2A\r")
        _write(fmt(model_alias, f"\x1b[32m{BLOCK_FULL * BAR_WIDTH} done \x1b[0m"))
        _write("\x1b[K\n\x1b[1B")

    return {"on_progress": on_progress, "finalize": finalize}


# ── Chat / Audio Streaming UI ─────────────────────────────────────────────

def print_user_msg(text):
    c = _cols()
    lines = wrap_text(text, min(c - 8, 60))
    for line in lines:
        print(f"  {line}")
    print()


def create_stream_box():
    c = _cols()
    box_width = min(c - 8, 60)

    def draw_row(text, cursor=False):
        display = text + "▍" if cursor else text
        return f"  │ {display:<{box_width}} │"

    print(f"  ┌{'─' * (box_width + 2)}┐")
    _write(draw_row("", True))

    state = {"current_line": "", "word_buf": ""}

    def flush_line():
        _write(f"\r{draw_row(state['current_line'])}\x1b[K\n")
        state["current_line"] = ""

    def push_word(word):
        cl = state["current_line"]
        if not cl:
            state["current_line"] = word
        elif len(cl) + 1 + len(word) <= box_width:
            state["current_line"] = cl + " " + word
        else:
            flush_line()
            state["current_line"] = word
        while len(state["current_line"]) > box_width:
            chunk = state["current_line"][:box_width]
            _write(f"\r{draw_row(chunk)}\x1b[K\n")
            state["current_line"] = state["current_line"][box_width:]

    def write(char):
        if char == "\n":
            if state["word_buf"]:
                push_word(state["word_buf"]); state["word_buf"] = ""
            flush_line()
            _write(f"\r{draw_row('', True)}\x1b[K")
        elif char == " ":
            if state["word_buf"]:
                push_word(state["word_buf"]); state["word_buf"] = ""
            _write(f"\r{draw_row(state['current_line'], True)}\x1b[K")
        else:
            state["word_buf"] += char
            preview = (state["current_line"] + " " + state["word_buf"]
                       if state["current_line"] else state["word_buf"])
            _write(f"\r{draw_row(preview, True)}\x1b[K")

    def finish():
        if state["word_buf"]:
            push_word(state["word_buf"]); state["word_buf"] = ""
        _write(f"\r{draw_row(state['current_line'])}\x1b[K\n")
        print(f"  └{'─' * (box_width + 2)}┘\n")

    return {"write": write, "finish": finish}


# ── User Input ────────────────────────────────────────────────────────────

def ask_user(prompt="  \x1b[36m> \x1b[0m"):
    try:
        return input(prompt)
    except (EOFError, KeyboardInterrupt):
        print()
        return "/quit"
