#!/usr/bin/env python3
"""
deepseek_buddy_bridge.py

Drive the M5StickC "buddy" from a local DeepSeek (or any OpenAI-compatible)
LLM server — Ollama, llama.cpp, vLLM, LM Studio, sglang, whatever.

How it works
------------
A tiny HTTP proxy speaks the OpenAI API. Point your client (aider, Continue,
Cline, open-webui, your own script, a browser) at http://localhost:11500/v1
instead of the real backend. The proxy forwards every request upstream,
counts tokens as they stream back, and pushes a one-line JSON status update
to the Stick every 500 ms over USB serial.

Install
-------
    pip install requests pyserial

Run
---
    # Ollama upstream (default :11434), Stick on COM6 (Windows):
    python deepseek_buddy_bridge.py --serial COM6

    # Linux / macOS:
    python deepseek_buddy_bridge.py --serial /dev/ttyUSB0

    # Custom backend + custom proxy port:
    python deepseek_buddy_bridge.py \\
        --backend http://localhost:8000/v1 \\
        --listen  :11500 \\
        --serial COM6
"""

import argparse
import json
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import requests

try:
    import serial
except ImportError:
    serial = None


# ─────────────────────────── shared state ───────────────────────────


class State:
    def __init__(self):
        self.lock            = threading.Lock()
        self.running         = 0
        self.waiting         = 0
        self.total           = 1
        self.tokens          = 0
        self.tokens_today    = 0
        self.day             = time.strftime("%Y-%m-%d")
        self.last_done       = 0.0
        self.last_tps10      = 0
        self.ctx             = 0
        self.ctx_max         = 0
        self.model           = ""
        self.log             = deque(maxlen=8)
        self.last_activity   = 0.0

    def _roll_day(self):
        d = time.strftime("%Y-%m-%d")
        if d != self.day:
            self.day = d
            self.tokens_today = 0

    def note_request(self, model, prompt_tokens, ctx_max=0):
        with self.lock:
            self.running += 1
            if model:
                self.model = model
            self.ctx = prompt_tokens or 0
            if ctx_max:
                self.ctx_max = ctx_max
            self.last_activity = time.time()
            self.log.append(f"> {self.model or 'llm'} · prompt {self.ctx}t")

    def note_done(self, completion_tokens, elapsed):
        with self.lock:
            self.running = max(0, self.running - 1)
            self.last_done = time.time()
            self.last_tps10 = int((completion_tokens / elapsed) * 10) if elapsed > 0 else 0
            self.tokens += completion_tokens
            self._roll_day()
            self.tokens_today += completion_tokens
            self.log.append(f"< {completion_tokens}t in {elapsed:.1f}s · "
                            f"{completion_tokens / max(elapsed, 1e-3):.0f} t/s")

    def snapshot(self):
        with self.lock:
            completed = (time.time() - self.last_done) < 2.0
            return {
                "total":        max(self.total, 1),
                "running":      self.running,
                "waiting":      self.waiting,
                "completed":    completed,
                "tokens":       self.tokens,
                "tokens_today": self.tokens_today,
                "model":        self.model[:23],
                "ctx":          min(self.ctx, 65535),
                "ctx_max":      min(self.ctx_max, 65535),
                "tps10":        self.last_tps10,
                "msg":          self._msg(),
                "entries":      list(self.log),
            }

    def _msg(self):
        if self.running:
            return f"{self.model or 'deepseek'} running"
        if self.model:
            return f"{self.model} idle"
        return "deepseek idle"


S = State()


# ─────────────────────── token counting helpers ───────────────────────


def _approx_prompt_tokens(req):
    total = 0
    for m in req.get("messages") or []:
        c = m.get("content")
        if isinstance(c, str):
            total += len(c)
        elif isinstance(c, list):
            for part in c:
                if isinstance(part, dict) and isinstance(part.get("text"), str):
                    total += len(part["text"])
    if isinstance(req.get("prompt"), str):
        total += len(req["prompt"])
    return total // 4          # ~4 chars/token, good enough for a pet


def _count_sse_line(line):
    if not line.startswith(b"data:"):
        return 0
    payload = line[5:].strip()
    if not payload or payload == b"[DONE]":
        return 0
    try:
        obj = json.loads(payload)
    except Exception:
        return 0
    delta = (obj.get("choices") or [{}])[0].get("delta") or {}
    text = delta.get("content") or ""
    if not text:
        return 0
    return max(1, len(text) // 4)


def _count_full_response(body):
    try:
        obj = json.loads(body)
    except Exception:
        return 0
    usage = obj.get("usage") or {}
    if usage.get("completion_tokens"):
        return int(usage["completion_tokens"])
    text = ""
    for ch in obj.get("choices") or []:
        msg = ch.get("message") or {}
        text += msg.get("content") or ""
    return len(text) // 4


# ─────────────────────────── proxy handler ───────────────────────────


class ProxyHandler(BaseHTTPRequestHandler):
    upstream = ""

    def log_message(self, *a):
        pass

    # ── CORS: browser preflight ───────────────────────────────
    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        self.send_header("Access-Control-Max-Age", "86400")
        self.end_headers()

    def do_GET(self):
        self._proxy("GET", b"")

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        self._proxy("POST", self.rfile.read(n) if n else b"")

    def _proxy(self, method, body):
        req_json, stream = {}, False
        if body:
            try:
                req_json = json.loads(body)
                stream = bool(req_json.get("stream"))
            except Exception:
                pass

        S.note_request(req_json.get("model", ""), _approx_prompt_tokens(req_json))

        url = self.upstream + self.path
        fwd = {k: v for k, v in self.headers.items()
               if k.lower() not in ("host", "content-length", "connection",
                                    "accept-encoding", "origin")}
        try:
            up = requests.request(method, url, data=body or None,
                                  headers=fwd, stream=True, timeout=900)
        except Exception as e:
            S.note_done(0, 1e-3)
            self.send_error(502, f"upstream: {e}")
            return

        self.send_response(up.status_code)
        # ── CORS: allow browser origin ────────────────────────
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        # ── forward upstream headers ──────────────────────────
        for k, v in up.headers.items():
            if k.lower() in ("transfer-encoding", "content-length",
                             "connection", "content-encoding",
                             "access-control-allow-origin",
                             "access-control-allow-headers"):
                continue
            self.send_header(k, v)
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()

        def emit(data, last=False):
            if data:
                self.wfile.write(b"%x\r\n" % len(data) + data + b"\r\n")
            if last:
                self.wfile.write(b"0\r\n\r\n")

        t0, produced, sse_buf = time.time(), 0, b""
        try:
            if stream:
                for chunk in up.iter_content(chunk_size=None):
                    if not chunk:
                        continue
                    sse_buf += chunk
                    while b"\n" in sse_buf:
                        line, sse_buf = sse_buf.split(b"\n", 1)
                        produced += _count_sse_line(line)
                    emit(chunk)
                if sse_buf:
                    produced += _count_sse_line(sse_buf)
            else:
                full = up.content
                produced = _count_full_response(full)
                emit(full)
        finally:
            emit(b"", last=True)
            S.note_done(produced, time.time() - t0)


# ────────────────────────── model discovery ──────────────────────────


def poll_models(stop, backend):
    while not stop.is_set():
        try:
            r = requests.get(backend + "/models", timeout=3)
            models = r.json().get("data", [])
            with S.lock:
                S.total = max(len(models), 1)
                if models and not S.model:
                    S.model = (models[0].get("id") or "")[:23]
        except Exception:
            pass
        stop.wait(10)


# ────────────────────────── serial writer ──────────────────────────


def send_json(ser, obj):
    ser.write((json.dumps(obj, separators=(",", ":")) + "\n").encode("utf-8"))


def serial_writer(stop, port, baud):
    if serial is None:
        print("pyserial not installed — `pip install pyserial`", file=sys.stderr)
        return

    while not stop.is_set():
        try:
            ser = serial.Serial(port, baud, timeout=1)
        except Exception as e:
            print(f"[serial] open failed: {e} — retrying in 3s", file=sys.stderr)
            stop.wait(3)
            continue

        print(f"[serial] opened {port} @ {baud}")
        time.sleep(2)                                   # let the ESP32 settle

        # Time sync first so the RTC is valid before the clock face runs.
        try:
            send_json(ser, {"time": [int(time.time()), -time.timezone]})
        except Exception as e:
            print(f"[serial] time-sync write failed: {e}", file=sys.stderr)

        last = 0.0
        while not stop.is_set():
            stop.wait(0.1)
            if time.time() - last < 0.5:
                continue
            last = time.time()
            try:
                send_json(ser, S.snapshot())
            except Exception as e:
                print(f"[serial] write failed: {e} — reopening", file=sys.stderr)
                try: ser.close()
                except Exception: pass
                break
        try: ser.close()
        except Exception: pass


# ──────────────────────────── entrypoint ────────────────────────────


def parse_listen(s, default_port):
    if not s:
        return "127.0.0.1", default_port
    if ":" in s:
        host, p = s.rsplit(":", 1)
        return (host or "127.0.0.1"), int(p)
    return "127.0.0.1", int(s)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--backend", default="http://localhost:11434",
                    help="OpenAI-compatible upstream base URL (default: Ollama)")
    ap.add_argument("--listen", default=":11500",
                    help="proxy listen address host:port (default: :11500)")
    ap.add_argument("--serial", required=True,
                    help="USB serial port for the Stick (e.g. COM6, /dev/ttyUSB0)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--poll-only", action="store_true",
                    help="don't run the proxy; just watch /v1/models and report idle")
    args = ap.parse_args()

    ProxyHandler.upstream = args.backend.rstrip("/")
    stop = threading.Event()

    threading.Thread(target=poll_models, args=(stop, ProxyHandler.upstream),
                     daemon=True).start()
    threading.Thread(target=serial_writer, args=(stop, args.serial, args.baud),
                     daemon=True).start()

    if args.poll_only:
        print(f"[poll] watching {ProxyHandler.upstream}/models — Ctrl-C to stop")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
        finally:
            stop.set()
        return

    host, port = parse_listen(args.listen, 11500)
    srv = ThreadingHTTPServer((host, port), ProxyHandler)
    print(f"[proxy] listening on http://{host}:{port}/v1")
    print(f"[proxy] forwarding to {ProxyHandler.upstream}")
    print(f"[hint ] browser chat: open chat.html")
    print(f"[hint ] code editors: OPENAI_BASE_URL=http://{host}:{port}/v1")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\n[proxy] shutting down")
    finally:
        stop.set()
        srv.server_close()


if __name__ == "__main__":
    main()