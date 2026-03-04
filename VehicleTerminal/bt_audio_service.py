#!/usr/bin/env python3
import argparse
import json
import os
import re
import shlex
import signal
import socket
import subprocess
import threading
import time
from dataclasses import dataclass
from typing import Dict, Optional, Tuple


@dataclass
class BtStatus:
    connected: bool = False
    device_name: str = ""
    device_mac: str = ""

    def as_dict(self) -> Dict[str, str]:
        if self.connected:
            return {
                "state": "connected",
                "deviceName": self.device_name,
                "deviceMac": self.device_mac,
            }
        return {
            "state": "disconnected",
            "deviceName": "",
            "deviceMac": "",
        }


class BtAudioService:
    def __init__(self, socket_path: str, sink_keyword: str, poll_interval: float = 2.0):
        self.socket_path = socket_path
        self.sink_keyword = sink_keyword.lower().strip()
        self.poll_interval = poll_interval

        self._running = False
        self._discoverable = False
        self._status = BtStatus()

        self._server_sock: Optional[socket.socket] = None
        self._clients = set()
        self._clients_lock = threading.Lock()

        self._stop_evt = threading.Event()
        self._poll_thread: Optional[threading.Thread] = None

    def run(self):
        self._prepare_socket()
        self._running = True

        self._poll_thread = threading.Thread(target=self._poll_loop, daemon=True)
        self._poll_thread.start()

        while not self._stop_evt.is_set():
            try:
                conn, _ = self._server_sock.accept()
                t = threading.Thread(target=self._handle_client, args=(conn,), daemon=True)
                t.start()
            except OSError:
                break

        self._cleanup()

    def stop_service(self):
        self._stop_evt.set()
        self._running = False
        self._apply_bt_discoverable(False)

        if self._server_sock:
            try:
                self._server_sock.close()
            except OSError:
                pass

    def _prepare_socket(self):
        if os.path.exists(self.socket_path):
            os.remove(self.socket_path)

        self._server_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._server_sock.bind(self.socket_path)
        self._server_sock.listen(8)
        os.chmod(self.socket_path, 0o666)

    def _cleanup(self):
        with self._clients_lock:
            for c in list(self._clients):
                try:
                    c.close()
                except OSError:
                    pass
            self._clients.clear()

        if os.path.exists(self.socket_path):
            os.remove(self.socket_path)

    def _run_cmd(self, cmd: str, timeout: int = 5) -> Tuple[int, str, str]:
        try:
            p = subprocess.run(
                shlex.split(cmd),
                capture_output=True,
                text=True,
                timeout=timeout,
            )
            return p.returncode, p.stdout.strip(), p.stderr.strip()
        except subprocess.TimeoutExpired:
            return 124, "", "timeout"

    def _run_btctl(self, bt_cmd: str, timeout: int = 8) -> Tuple[int, str, str]:
        cmd = f"bluetoothctl --timeout {timeout} {bt_cmd}"
        return self._run_cmd(cmd, timeout=timeout + 2)

    def _apply_bt_discoverable(self, enabled: bool) -> bool:
        steps = [
            "power on",
            "agent on",
            "default-agent",
            "pairable on" if enabled else "pairable off",
            "discoverable on" if enabled else "discoverable off",
            "scan on" if enabled else "scan off",
        ]

        ok = True
        for step in steps:
            code, _, _ = self._run_btctl(step)
            if code != 0:
                ok = False

        self._discoverable = enabled and ok
        return ok

    def _get_connected_device(self) -> BtStatus:
        code, out, _ = self._run_btctl("devices Connected")
        if code != 0 or not out.strip():
            return BtStatus(False, "", "")

        line = out.strip().splitlines()[0].strip()
        m = re.match(r"Device\s+([0-9A-F:]{17})\s+(.+)", line, re.IGNORECASE)
        if not m:
            return BtStatus(False, "", "")

        mac = m.group(1)
        name = m.group(2).strip()
        return BtStatus(True, name, mac)

    def _best_es8388_sink(self) -> Optional[str]:
        code, out, _ = self._run_cmd("pactl list short sinks")
        if code != 0:
            return None

        lines = [line.strip() for line in out.splitlines() if line.strip()]
        if not lines:
            return None

        keyword = self.sink_keyword
        for line in lines:
            parts = line.split()
            sink_name = parts[1] if len(parts) > 1 else ""
            lowered = line.lower()
            if keyword and keyword in lowered:
                return sink_name

        fallback_keys = ["es8388", "rk", "rockchip", "analog", "headphone", "alsa_output"]
        for line in lines:
            parts = line.split()
            sink_name = parts[1] if len(parts) > 1 else ""
            lowered = line.lower()
            if any(k in lowered for k in fallback_keys):
                return sink_name

        parts = lines[0].split()
        return parts[1] if len(parts) > 1 else None

    def _set_bluetooth_profile_a2dp_sink(self):
        code, out, _ = self._run_cmd("pactl list cards short")
        if code != 0:
            return

        for line in out.splitlines():
            if "bluez_card" not in line:
                continue
            card_name = line.split()[1]
            self._run_cmd(f"pactl set-card-profile {card_name} a2dp-sink")
            self._run_cmd(f"pactl set-card-profile {card_name} a2dp_sink")

    def _route_audio_to_es8388(self):
        sink = self._best_es8388_sink()
        if not sink:
            return

        self._run_cmd(f"pactl set-default-sink {sink}")

        code, out, _ = self._run_cmd("pactl list short sink-inputs")
        if code != 0:
            return

        for line in out.splitlines():
            cols = line.split()
            if not cols:
                continue
            input_id = cols[0]
            self._run_cmd(f"pactl move-sink-input {input_id} {sink}")

    def _poll_loop(self):
        last_state = BtStatus(False, "", "")
        while not self._stop_evt.is_set():
            try:
                status = self._get_connected_device()
                self._status = status

                if status.connected:
                    self._set_bluetooth_profile_a2dp_sink()
                    self._route_audio_to_es8388()

                if status.connected != last_state.connected or status.device_mac != last_state.device_mac:
                    self._broadcast_event("statusChanged", status.as_dict())
                    last_state = status
            except Exception as exc:
                self._broadcast_event("serviceError", {"message": str(exc)})

            time.sleep(self.poll_interval)

    def _send_json_line(self, conn: socket.socket, obj: Dict):
        payload = (json.dumps(obj, ensure_ascii=False) + "\n").encode("utf-8")
        conn.sendall(payload)

    def _broadcast_event(self, event: str, payload: Dict):
        msg = {"event": event, "payload": payload}
        stale = []
        with self._clients_lock:
            for c in list(self._clients):
                try:
                    self._send_json_line(c, msg)
                except OSError:
                    stale.append(c)
            for c in stale:
                self._clients.discard(c)
                try:
                    c.close()
                except OSError:
                    pass

    def _handle_client(self, conn: socket.socket):
        with self._clients_lock:
            self._clients.add(conn)

        try:
            self._send_json_line(conn, {
                "event": "hello",
                "payload": {
                    "service": "bt_audio_service",
                    "status": self._status.as_dict(),
                }
            })

            f = conn.makefile("r", encoding="utf-8")
            for raw in f:
                raw = raw.strip()
                if not raw:
                    continue
                try:
                    req = json.loads(raw)
                except json.JSONDecodeError:
                    self._send_json_line(conn, {"ok": False, "error": "invalid json"})
                    continue

                method = req.get("method", "")
                if method == "start":
                    ok = self._apply_bt_discoverable(True)
                    self._send_json_line(conn, {"ok": ok, "result": "started" if ok else "partial"})
                elif method == "stop":
                    ok = self._apply_bt_discoverable(False)
                    self._send_json_line(conn, {"ok": ok, "result": "stopped" if ok else "partial"})
                elif method == "getStatus":
                    self._send_json_line(conn, {"ok": True, "status": self._status.as_dict()})
                elif method == "ping":
                    self._send_json_line(conn, {"ok": True, "result": "pong"})
                else:
                    self._send_json_line(conn, {"ok": False, "error": "unknown method"})
        except Exception:
            pass
        finally:
            with self._clients_lock:
                self._clients.discard(conn)
            try:
                conn.close()
            except OSError:
                pass


def main():
    parser = argparse.ArgumentParser(description="Bluetooth A2DP sink + PulseAudio routing service")
    parser.add_argument("--socket", default="/tmp/bt_audio_service.sock", help="Unix socket path")
    parser.add_argument("--sink-keyword", default=os.getenv("BT_AUDIO_SINK_KEYWORD", "es8388"),
                        help="Keyword to match ES8388 sink in pactl list")
    args = parser.parse_args()

    service = BtAudioService(socket_path=args.socket, sink_keyword=args.sink_keyword)

    def _sig_handler(_signum, _frame):
        service.stop_service()

    signal.signal(signal.SIGINT, _sig_handler)
    signal.signal(signal.SIGTERM, _sig_handler)

    service.run()


if __name__ == "__main__":
    main()
