#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOCKET_PATH="/tmp/bt_audio_service.sock"
SINK_KEYWORD="es8388"
SKIP_BUILD=0

PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0
STARTED_PID=""

log_info() { echo "[INFO] $*"; }
log_pass() { echo "[PASS] $*"; PASS_COUNT=$((PASS_COUNT + 1)); }
log_fail() { echo "[FAIL] $*"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
log_warn() { echo "[WARN] $*"; WARN_COUNT=$((WARN_COUNT + 1)); }

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --skip-build              Skip qmake + make build check
  --socket <path>           Unix socket path (default: ${SOCKET_PATH})
  --sink-keyword <keyword>  Sink keyword for service (default: ${SINK_KEYWORD})
  -h, --help                Show help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --socket)
      SOCKET_PATH="$2"
      shift 2
      ;;
    --sink-keyword)
      SINK_KEYWORD="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
done

cleanup() {
  if [[ -n "$STARTED_PID" ]]; then
    log_info "Stopping temporary bt_audio_service (pid=$STARTED_PID)"
    kill "$STARTED_PID" >/dev/null 2>&1 || true
    wait "$STARTED_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

require_cmd() {
  local cmd="$1"
  if command -v "$cmd" >/dev/null 2>&1; then
    log_pass "Command available: $cmd"
  else
    log_fail "Command missing: $cmd"
  fi
}

log_info "==== Bluetooth Audio One-Click Verify ===="
log_info "Project: $ROOT_DIR"
log_info "Socket:  $SOCKET_PATH"

require_cmd python3
require_cmd bluetoothctl
require_cmd pactl
if [[ $SKIP_BUILD -eq 0 ]]; then
  require_cmd qmake
  require_cmd make
fi

if systemctl is-active --quiet bluetooth; then
  log_pass "bluetooth service is active"
else
  log_fail "bluetooth service is NOT active"
fi

if pulseaudio --check >/dev/null 2>&1; then
  log_pass "pulseaudio is running"
else
  log_warn "pulseaudio not running, trying to start"
  if pulseaudio --start >/dev/null 2>&1; then
    log_pass "pulseaudio started"
  else
    log_fail "failed to start pulseaudio"
  fi
fi

if python3 -m py_compile "$ROOT_DIR/bt_audio_service.py"; then
  log_pass "Python syntax OK: bt_audio_service.py"
else
  log_fail "Python syntax check failed: bt_audio_service.py"
fi

if [[ $SKIP_BUILD -eq 0 ]]; then
  log_info "Running qmake + make build verification"
  if (cd "$ROOT_DIR" && qmake VehicleTerminal.pro && make -j"$(nproc)"); then
    log_pass "Build OK (qmake + make)"
  else
    log_fail "Build failed"
  fi
else
  log_info "Build verification skipped (--skip-build)"
fi

log_info "Checking sink list"
SINKS_OUTPUT="$(pactl list short sinks 2>/dev/null || true)"
if [[ -n "$SINKS_OUTPUT" ]]; then
  log_pass "Found PulseAudio sinks"
  if echo "$SINKS_OUTPUT" | grep -qi "$SINK_KEYWORD"; then
    log_pass "Found sink keyword '$SINK_KEYWORD'"
  else
    log_warn "Sink keyword '$SINK_KEYWORD' not found; service will use fallback matching"
  fi
else
  log_fail "No PulseAudio sinks found"
fi

SERVICE_REUSED=0
if [[ -S "$SOCKET_PATH" ]]; then
  if python3 - <<PY >/dev/null 2>&1
import socket
s=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.settimeout(0.5)
s.connect("$SOCKET_PATH")
s.close()
PY
  then
    SERVICE_REUSED=1
    log_pass "Reusing existing service socket: $SOCKET_PATH"
  fi
fi

if [[ $SERVICE_REUSED -eq 0 ]]; then
  log_info "Starting temporary bt_audio_service for protocol tests"
  python3 "$ROOT_DIR/bt_audio_service.py" --socket "$SOCKET_PATH" --sink-keyword "$SINK_KEYWORD" \
    > /tmp/bt_audio_verify_service.log 2>&1 &
  STARTED_PID=$!
  sleep 1

  if [[ -S "$SOCKET_PATH" ]]; then
    log_pass "Service started and socket created"
  else
    log_fail "Service failed to create socket ($SOCKET_PATH)"
  fi
fi

log_info "Running socket protocol tests: ping/getStatus/start/stop"
if python3 - <<PY
import json, socket, time, sys
sock_path = "$SOCKET_PATH"

c = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
c.settimeout(1.0)
c.connect(sock_path)
_ = c.recv(4096)
rx_buf = ""

def recv_response_for(method, timeout_sec):
  global rx_buf
  deadline = time.time() + timeout_sec
  while time.time() < deadline:
    try:
      chunk = c.recv(4096).decode()
    except socket.timeout:
      continue
    if not chunk:
      continue
    rx_buf += chunk
    while "\n" in rx_buf:
      line, rx_buf = rx_buf.split("\n", 1)
      line = line.strip()
      if not line:
        continue
      try:
        obj = json.loads(line)
      except Exception:
        continue
      if "ok" not in obj:
        continue
      return obj
  raise TimeoutError(f"Timeout waiting response for {method}")


def call(method):
  c.sendall((json.dumps({"method": method}) + "\n").encode())
  if method in ("start", "stop"):
    obj = recv_response_for(method, 90)
  else:
    obj = recv_response_for(method, 6)

  if method == "ping":
    if not obj.get("ok") or obj.get("result") != "pong":
      print(f"Unexpected ping response: {obj}")
      sys.exit(3)
  if method == "getStatus":
    status = obj.get("status", {})
    if status.get("state") not in ("connected", "disconnected"):
      print(f"Unexpected getStatus response: {obj}")
      sys.exit(4)
  if method in ("start", "stop"):
    if not obj.get("ok", False):
      print(f"Method {method} returned not ok: {obj}")
      sys.exit(5)

for m in ["ping", "getStatus", "start", "getStatus", "stop"]:
    call(m)

c.close()
print("Socket API tests passed")
PY
then
  log_pass "Socket API protocol tests passed"
else
  log_fail "Socket API protocol tests failed"
fi

log_info "==== Summary ===="
log_info "PASS: $PASS_COUNT"
log_info "WARN: $WARN_COUNT"
log_info "FAIL: $FAIL_COUNT"

if [[ $FAIL_COUNT -eq 0 ]]; then
  echo
  log_pass "One-click verification finished successfully"
  echo "Manual final step: open app + phone playback for real audio path"
  echo "  cd $ROOT_DIR && ./VehicleTerminal"
  echo "  Then play music on phone and check: pactl list short sink-inputs"
  exit 0
fi

echo
log_fail "One-click verification finished with failures"
exit 1
