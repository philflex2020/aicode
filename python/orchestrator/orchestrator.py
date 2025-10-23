#!/usr/bin/env python3#!/usr/bin/env python3
# Orchestrate: upload -> start remote BG task -> run local host test -> stop remote -> evaluate
#
# Deps: pip install paramiko
#
# Usage (config file):
#   python3 orchestrate_target_test.py --config test_plan.json
#
# Or ad-hoc CLI (no file upload, simple host cmd & expectations):
#   python3 orchestrate_target_test.py \
#     --target root@192.168.86.26 --port 22 --password 'mypw' \
#     --remote-cmd '/tmp/mydaemon --arg foo' \
#     --host-cmd 'python3 can_ws_soc_test_py.py' \
#     --expect-host-contains 'PASS' --expect-host-not-contains 'FAIL' \
#     --expect-target-not-contains 'panic' --expect-target-not-contains 'ERROR'
#
# NOTE on "messy" \n: JSON strings always escape newlines. To make the output human-friendly,
# this script now ALSO includes `stdout_lines` / `stderr_lines` arrays in the JSON, and it can
# optionally echo a pretty version to STDERR with --pretty-to-stderr.

import argparse
import json
import re
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

import paramiko


# ----------------------------- Expect helpers -----------------------------

def expect_strings(
    text: str,
    contains: Optional[List[str]] = None,
    not_contains: Optional[List[str]] = None,
    regex: Optional[List[str]] = None,
) -> Dict[str, object]:
    """Evaluate simple expectations on a text blob."""
    contains = contains or []
    not_contains = not_contains or []
    regex = regex or []

    ok = True
    failures = []

    for s in contains:
        if s not in text:
            ok = False
            failures.append(f"missing required substring: {s!r}")

    for s in not_contains:
        if s in text:
            ok = False
            failures.append(f"found forbidden substring: {s!r}")

    for r in regex:
        try:
            if not re.search(r, text, flags=re.MULTILINE | re.DOTALL):
                ok = False
                failures.append(f"regex not matched: /{r}/")
        except re.error as e:
            ok = False
            failures.append(f"bad regex /{r}/: {e}")

    return {"pass": ok, "failures": failures, "length": len(text)}


# ----------------------------- SSH helper -----------------------------

@dataclass
class SSHTarget:
    host: str
    user: str = "root"
    port: int = 22
    password: Optional[str] = None
    keyfile: Optional[str] = None
    timeout: float = 10.0

    client: Optional[paramiko.SSHClient] = field(default=None, init=False)

    def connect(self):
        if self.client:
            return
        self.client = paramiko.SSHClient()
        self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        kwargs = {
            "hostname": self.host,
            "port": self.port,
            "username": self.user,
            "timeout": self.timeout,
            "allow_agent": True,
            "look_for_keys": True,
        }
        if self.password:
            kwargs["password"] = self.password
        else:
            kwargs["password"] = ""

        if self.keyfile:
            kwargs["key_filename"] = self.keyfile

        self.client.connect(**kwargs)

    def close(self):
        if self.client:
            try:
                self.client.close()
            finally:
                self.client = None

    def sftp(self):
        assert self.client, "SSH not connected"
        return self.client.open_sftp()

    def run(self, cmd: str, get_pty: bool = False, timeout: Optional[float] = None) -> Tuple[int, str, str]:
        """Run a command and return (rc, stdout, stderr)."""
        assert self.client, "SSH not connected"
        stdin, stdout, stderr = self.client.exec_command(cmd, get_pty=get_pty, timeout=timeout)
        out = stdout.read().decode(errors="replace")
        err = stderr.read().decode(errors="replace")
        rc = stdout.channel.recv_exit_status()
        return rc, out, err


# ----------------------------- Orchestrator -----------------------------

@dataclass
class Plan:
    # Target connection
    target: str  # user@host or host
    port: int = 22
    password: Optional[str] = None
    keyfile: Optional[str] = None
    timeout: float = 10.0

    # Background program (optional upload)
    upload_src: Optional[str] = None       # local path to upload
    upload_dest: Optional[str] = None      # remote destination filename
    chmod_x: bool = True
    remote_cmd: Optional[str] = None       # the command to run in background
    workdir: Optional[str] = None          # remote working directory (cd first)
    env: Dict[str, str] = field(default_factory=dict)
    pidfile: str = "/tmp/orch_bg.pid"
    logfile: str = "/tmp/orch_bg.log"
    stop_signal: str = "TERM"              # TERM then KILL fallback
    stop_timeout: float = 5.0

    # Host test
    host_cmd: Optional[str] = None
    host_timeout: Optional[float] = None   # seconds

    # Expect
    expect_host_contains: List[str] = field(default_factory=list)
    expect_host_not_contains: List[str] = field(default_factory=list)
    expect_host_regex: List[str] = field(default_factory=list)

    expect_target_contains: List[str] = field(default_factory=list)
    expect_target_not_contains: List[str] = field(default_factory=list)
    expect_target_regex: List[str] = field(default_factory=list)

    # Delays
    post_start_delay: float = 0.5          # wait after starting remote bg before host test

    def parse_target(self) -> Tuple[str, str]:
        """Return (user, host) given target string."""
        t = self.target.strip()
        if "@" in t:
            user, host = t.split("@", 1)
        else:
            user, host = "root", t
        return user, host


# ----------------------------- Text tidy helpers -----------------------------

def _tidy_embedded_newlines(text: str) -> str:
    """
    Normalize/pretty-print outputs that contain *escaped* newline sequences.

    Converts:
      - Windows newlines to LF
      - Literal backslash-escaped CRLF ('\\r\\n') to newline
      - Literal backslash-escaped LF ('\\n') to newline
      - Literal backslash-escaped CR ('\\r') to newline
    Leaves other escape sequences alone to avoid over-decoding.
    """
    if not text:
        return text
    # Normalize real CRLF first
    t = text.replace("\r\n", "\n")
    # Then handle escaped sequences
    t = t.replace("\\r\\n", "\n")
    t = t.replace("\\n", "\n")
    t = t.replace("\\r", "\n")
    return t


def _combine_and_tidy(host_out: str, host_err: str) -> Tuple[str, str, str]:
    """Return (tidy_out, tidy_err, tidy_both_for_expect)."""
    tidy_out = _tidy_embedded_newlines(host_out or "")
    tidy_err = _tidy_embedded_newlines(host_err or "")
    # For expectations, evaluate against combined output (stdout + stderr)
    tidy_both = tidy_out + ("\n" + tidy_err if tidy_err else "")
    return tidy_out, tidy_err, tidy_both


def start_remote_background(t: SSHTarget, plan: Plan) -> Tuple[bool, str]:
    """
    Start remote background process with nohup & capture PID, write to pidfile and log.
    Returns (ok, pid_str_or_error).
    """
    # Build environment prefix
    env_prefix = ""
    if plan.env:
        env_prefix = " ".join([f'{shlex.quote(k)}={shlex.quote(v)}' for k, v in plan.env.items()]) + " "

    # Optional cd
    cd_prefix = ""
    if plan.workdir:
        cd_prefix = f"cd {shlex.quote(plan.workdir)} && "
    log_prefix = ""
    if plan.logfile:
        log_prefix = f"rm -f {shlex.quote(plan.logfile)} && "

    # Command to run
    if not plan.remote_cmd:
        return False, "remote_cmd is required to start background task"

    # Compose the nohup launch; print PID to stdout so we can grab it
    # Redirect stdin from /dev/null to avoid holding the channel open.
    cmd = (
        f"{cd_prefix}"
        f"{log_prefix}"
        f"sh -lc {shlex.quote(env_prefix + f'nohup {plan.remote_cmd} >{plan.logfile} 2>&1 < /dev/null & echo $!')}"
    )

    rc, out, err = t.run(cmd)
    if rc != 0:
        return False, f"remote start failed rc={rc}, err={err.strip()}"

    pid = out.strip().splitlines()[-1].strip() if out.strip() else ""
    if not pid.isdigit():
        # Some shells may emit extra text; try last numeric token
        toks = re.findall(r"\b(\d+)\b", out)
        pid = toks[-1] if toks else ""

    if not pid:
        return False, f"could not parse PID from: {out!r} / {err!r}"

    # Save pidfile
    rc2, _, err2 = t.run(f"printf %s {shlex.quote(pid)} > {shlex.quote(plan.pidfile)}")
    if rc2 != 0:
        return False, f"failed to write pidfile: {err2.strip()}"

    return True, pid


def stop_remote_background(t: SSHTarget, plan: Plan) -> Tuple[bool, str]:
    """
    Stop remote background by reading pidfile and signaling the process.
    TERM -> wait -> if still alive, KILL.
    """
    # Read PID
    rc, out, err = t.run(f"cat {shlex.quote(plan.pidfile)} 2>/dev/null || true")
    pid = (out or "").strip()
    if not pid.isdigit():
        return False, "no pidfile / invalid PID"

    # Send TERM
    rc, _, _ = t.run(f"kill -{plan.stop_signal} {pid} 2>/dev/null || true")
    # Wait a bit and see if still alive
    deadline = time.time() + plan.stop_timeout
    while time.time() < deadline:
        rc, out, _ = t.run(f"kill -0 {pid} 2>/dev/null; echo $?")
        alive = (out.strip().splitlines()[-1].strip() == "0")
        if not alive:
            break
        time.sleep(0.25)

    # If still alive, KILL
    rc, out, _ = t.run(f"kill -0 {pid} 2>/dev/null; echo $?")
    alive = (out.strip().splitlines()[-1].strip() == "0")
    if alive:
        t.run(f"kill -KILL {pid} 2>/dev/null || true")

    return True, pid


def run_host_cmd(cmd: str, timeout: Optional[float]) -> Tuple[int, str, str]:
    """
    Run a local host command, capture rc/stdout/stderr.
    (Raw capture; tidying happens in run_plan so expectations see prettified text.)
    """
    proc = subprocess.Popen(
        cmd,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        out, err = proc.communicate(timeout=timeout)
        return proc.returncode, out, err
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
        return 124, out, (err or "") + "\nTIMEOUT"


def run_plan(plan: Plan) -> Dict[str, object]:
    # Connect SSH
    user, host = plan.parse_target()
    ssh = SSHTarget(
        host=host,
        user=user,
        port=plan.port,
        password=plan.password,
        keyfile=plan.keyfile,
        timeout=plan.timeout,
    )

    remote_started = False
    remote_pid = ""
    uploaded = None

    try:
        ssh.connect()

        # 1) Optional upload
        if plan.upload_src and plan.upload_dest:
            with ssh.sftp() as s:
                s.put(plan.upload_src, plan.upload_dest)
            uploaded = plan.upload_dest
            if plan.chmod_x:
                ssh.run(f"chmod a+x {shlex.quote(plan.upload_dest)}")

        # 2) Start remote BG
        ok, info = start_remote_background(ssh, plan)
        if not ok:
            return {"ok": False, "stage": "start_remote", "error": info}
        remote_started = True
        remote_pid = info

        # 3) Small wait to let it initialize
        time.sleep(plan.post_start_delay)

        # 4) Run host test (optional)
        host_rc = None
        host_out = ""
        host_err = ""
        host_expect = {}

        if plan.host_cmd:
            host_rc, host_out_raw, host_err_raw = run_host_cmd(plan.host_cmd, plan.host_timeout)
            # Tidy embedded newlines in the host outputs so \n -> actual newlines
            host_out, host_err, host_full_for_expect = _combine_and_tidy(host_out_raw, host_err_raw)
            # Evaluate expectations on *tidied* host stdout+stderr
            host_expect = expect_strings(
                host_full_for_expect,
                contains=plan.expect_host_contains,
                not_contains=plan.expect_host_not_contains,
                regex=plan.expect_host_regex,
            )
        else:
            host_out = ""
            host_err = ""
            host_rc = 0
            host_expect = {"pass": True, "failures": [], "length": 0}

        # 5) Stop remote BG
        stop_ok, stop_info = stop_remote_background(ssh, plan)

        # 6) Fetch remote log for expectations
        remote_log_text = ""
        try:
            with ssh.sftp() as s:
                with s.open(plan.logfile, "r") as fh:
                    remote_log_text = fh.read().decode("utf-8", errors="replace")
        except Exception:
            # log may not exist; tolerate
            pass

        target_expect = expect_strings(
            remote_log_text,
            contains=plan.expect_target_contains,
            not_contains=plan.expect_target_not_contains,
            regex=plan.expect_target_regex,
        )

        # Overall pass: remote started + stopped + (if host_cmd) host_rc==0 + both expectations pass
        overall_ok = True
        if not remote_started:
            overall_ok = False
        if plan.host_cmd and (host_rc is None or host_rc != 0):
            overall_ok = False
        if host_expect.get("pass") is False:
            overall_ok = False
        if target_expect.get("pass") is False:
            overall_ok = False

        return {
            "ok": overall_ok,
            "remote": {
                "started": remote_started, "pid": remote_pid,
                "stopped": stop_ok, "stop_info": stop_info
            },
            "upload": {"dest": uploaded},
            "host": {
                "rc": host_rc,
                # Keep text fields for compatibility (JSON will escape newlines),
                # and ALSO provide line arrays for human-friendly inspection.
                "stdout": host_out,
                "stdout_lines": host_out.splitlines(),
                "stderr": host_err,
                "stderr_lines": host_err.splitlines(),
                "expect": host_expect
            },
            "target_log": {
                "path": plan.logfile,
                "length": len(remote_log_text),
                "sample": remote_log_text[-2048:],
                "sample_lines": remote_log_text[-2048:].splitlines()
            },
            "target_expect": target_expect,
        }

    except Exception as e:
        return {"ok": False, "stage": "exception", "error": str(e)}
    finally:
        try:
            ssh.close()
        except Exception:
            pass


# ----------------------------- CLI -----------------------------

def load_plan_from_file(path: str) -> Plan:
    with open(path, "r", encoding="utf-8") as f:
        j = json.load(f)

    # Simple mapping with defaults, tolerating missing keys
    def g(*ks, default=None):
        d = j
        for k in ks:
            d = d.get(k, {})
        return d if d else default

    return Plan(
        target = j.get("target") or "root@127.0.0.1",
        port   = j.get("port", 22),
        password = j.get("password"),
        keyfile  = j.get("keyfile"),
        timeout  = j.get("timeout", 10.0),

        upload_src  = g("background", "upload", "src", default=j.get("upload_src")),
        upload_dest = g("background", "upload", "dest", default=j.get("upload_dest")),
        chmod_x     = g("background", "chmod_x", default=j.get("chmod_x", True)),
        remote_cmd  = g("background", "cmd", default=j.get("remote_cmd")),
        workdir     = g("background", "workdir", default=j.get("workdir")),
        env         = g("background", "env", default=j.get("env", {})) or {},
        pidfile     = g("background", "pidfile", default=j.get("pidfile", "/tmp/orch_bg.pid")),
        logfile     = g("background", "logfile", default=j.get("logfile", "/tmp/orch_bg.log")),
        stop_signal = g("background", "stop_signal", default=j.get("stop_signal", "TERM")),
        stop_timeout= g("background", "stop_timeout", default=j.get("stop_timeout", 5.0)),

        host_cmd    = g("host_test", "cmd", default=j.get("host_cmd")),
        host_timeout= g("host_test", "timeout", default=j.get("host_timeout")),

        expect_host_contains = g("expect", "host", "contains", default=j.get("expect_host_contains", [])) or [],
        expect_host_not_contains = g("expect", "host", "not_contains", default=j.get("expect_host_not_contains", [])) or [],
        expect_host_regex = g("expect", "host", "regex", default=j.get("expect_host_regex", [])) or [],

        expect_target_contains = g("expect", "target", "contains", default=j.get("expect_target_contains", [])) or [],
        expect_target_not_contains = g("expect", "target", "not_contains", default=j.get("expect_target_not_contains", [])) or [],
        expect_target_regex = g("expect", "target", "regex", default=j.get("expect_target_regex", [])) or [],

        post_start_delay = j.get("post_start_delay", 0.5),
    )


def main():
    ap = argparse.ArgumentParser(description="Orchestrate remote BG task + host test + cleanup")
    ap.add_argument("--config", default="orch/orc_plan.json", help="JSON plan file")
    ap.add_argument("--target", help="user@host or host")
    ap.add_argument("--port", type=int, default=22)
    ap.add_argument("--password")
    ap.add_argument("--keyfile")

    ap.add_argument("--upload-src")
    ap.add_argument("--upload-dest")
    ap.add_argument("--no-chmod", action="store_true")

    ap.add_argument("--remote-cmd", help="Command to run in background on target")
    ap.add_argument("--workdir")
    ap.add_argument("--pidfile", default="/tmp/orch_bg.pid")
    ap.add_argument("--logfile", default="/tmp/orch_bg.log")

    ap.add_argument("--host-cmd", help="Local host command to run")
    ap.add_argument("--host-timeout", type=float)

    ap.add_argument("--expect-host-contains", action="append", default=[])
    ap.add_argument("--expect-host-not-contains", action="append", default=[])
    ap.add_argument("--expect-host-regex", action="append", default=[])

    ap.add_argument("--expect-target-contains", action="append", default=[])
    ap.add_argument("--expect-target-not-contains", action="append", default=[])
    ap.add_argument("--expect-target-regex", action="append", default=[])

    ap.add_argument("--post-start-delay", type=float, default=0.5)

    # New: echo pretty host stdout to STDERR after printing JSON (doesn't break JSON parsers of STDOUT)
    ap.add_argument("--pretty-to-stderr", action="store_true",
                    help="After JSON, echo prettified host stdout to STDERR for human reading")

    args = ap.parse_args()

    if args.config:
        plan = load_plan_from_file(args.config)
    else:
        if not args.target:
            ap.error("--target or --config required")
        plan = Plan(
            target=args.target, port=args.port, password=args.password, keyfile=args.keyfile,
            upload_src=args.upload_src, upload_dest=args.upload_dest, chmod_x=not args.no_chmod,
            remote_cmd=args.remote_cmd, workdir=args.workdir, pidfile=args.pidfile, logfile=args.logfile,
            host_cmd=args.host_cmd, host_timeout=args.host_timeout,
            expect_host_contains=args.expect_host_contains,
            expect_host_not_contains=args.expect_host_not_contains,
            expect_host_regex=args.expect_host_regex,
            expect_target_contains=args.expect_target_contains,
            expect_target_not_contains=args.expect_target_not_contains,
            expect_target_regex=args.expect_target_regex,
            post_start_delay=args.post_start_delay,
        )

    # Sanity
    if not plan.remote_cmd:
        print("ERROR: remote_cmd is required (either in --remote-cmd or config.background.cmd)", file=sys.stderr)
        sys.exit(2)

    res = run_plan(plan)
    # Keep JSON clean on STDOUT
    print(json.dumps(res, indent=2, ensure_ascii=False))

    # Optional pretty echo to STDERR so humans can read the test output with real newlines
    if args.pretty_to_stderr and "host" in res:
        sys.stderr.write("\n===== HOST STDOUT (pretty) =====\n")
        sys.stderr.write((res["host"].get("stdout") or "").rstrip() + "\n")
        sys.stderr.write("===== HOST STDERR (pretty) =====\n")
        sys.stderr.write((res["host"].get("stderr") or "").rstrip() + "\n")

    sys.exit(0 if res.get("ok") else 1)


if __name__ == "__main__":
    main()


# #!/usr/bin/env python3
# # Orchestrate: upload -> start remote BG task -> run local host test -> stop remote -> evaluate
# #
# # Deps: pip install paramiko
# #
# # Usage (config file):
# #   python3 orchestrate_target_test.py --config test_plan.json
# #
# # Or ad-hoc CLI (no file upload, simple host cmd & expectations):
# #   python3 orchestrate_target_test.py \
# #     --target root@192.168.86.26 --port 22 --password 'mypw' \
# #     --remote-cmd '/tmp/mydaemon --arg foo' \
# #     --host-cmd 'python3 can_ws_soc_test_py.py' \
# #     --expect-host-contains 'PASS' --expect-host-not-contains 'FAIL' \
# #     --expect-target-not-contains 'panic' --expect-target-not-contains 'ERROR'

# import argparse
# import json
# import os
# import re
# import shlex
# import subprocess
# import sys
# import time
# from dataclasses import dataclass, field
# from typing import Dict, List, Optional, Tuple

# import paramiko


# # ----------------------------- Expect helpers -----------------------------

# def expect_strings(
#     text: str,
#     contains: Optional[List[str]] = None,
#     not_contains: Optional[List[str]] = None,
#     regex: Optional[List[str]] = None,
# ) -> Dict[str, object]:
#     """Evaluate simple expectations on a text blob."""
#     contains = contains or []
#     not_contains = not_contains or []
#     regex = regex or []

#     ok = True
#     failures = []

#     for s in contains:
#         if s not in text:
#             ok = False
#             failures.append(f"missing required substring: {s!r}")

#     for s in not_contains:
#         if s in text:
#             ok = False
#             failures.append(f"found forbidden substring: {s!r}")

#     for r in regex:
#         try:
#             if not re.search(r, text, flags=re.MULTILINE | re.DOTALL):
#                 ok = False
#                 failures.append(f"regex not matched: /{r}/")
#         except re.error as e:
#             ok = False
#             failures.append(f"bad regex /{r}/: {e}")

#     return {"pass": ok, "failures": failures, "length": len(text)}


# # ----------------------------- SSH helper -----------------------------

# @dataclass
# class SSHTarget:
#     host: str
#     user: str = "root"
#     port: int = 22
#     password: Optional[str] = None
#     keyfile: Optional[str] = None
#     timeout: float = 10.0

#     client: Optional[paramiko.SSHClient] = field(default=None, init=False)

#     def connect(self):
#         if self.client:
#             return
#         self.client = paramiko.SSHClient()
#         self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

#         kwargs = {
#             "hostname": self.host,
#             "port": self.port,
#             "username": self.user,
#             "timeout": self.timeout,
#             "allow_agent": True,
#             "look_for_keys": True,
#         }
#         if self.password:
#             kwargs["password"] = self.password
#         else:
#             kwargs["password"] = ""

#         if self.keyfile:
#             kwargs["key_filename"] = self.keyfile

#         self.client.connect(**kwargs)

#     def close(self):
#         if self.client:
#             try:
#                 self.client.close()
#             finally:
#                 self.client = None

#     def sftp(self):
#         assert self.client, "SSH not connected"
#         return self.client.open_sftp()

#     def run(self, cmd: str, get_pty: bool = False, timeout: Optional[float] = None) -> Tuple[int, str, str]:
#         """Run a command and return (rc, stdout, stderr)."""
#         assert self.client, "SSH not connected"
#         stdin, stdout, stderr = self.client.exec_command(cmd, get_pty=get_pty, timeout=timeout)
#         out = stdout.read().decode(errors="replace")
#         err = stderr.read().decode(errors="replace")
#         rc = stdout.channel.recv_exit_status()
#         return rc, out, err


# # ----------------------------- Orchestrator -----------------------------

# @dataclass
# class Plan:
#     # Target connection
#     target: str  # user@host or host
#     port: int = 22
#     password: Optional[str] = None
#     keyfile: Optional[str] = None
#     timeout: float = 10.0

#     # Background program (optional upload)
#     upload_src: Optional[str] = None       # local path to upload
#     upload_dest: Optional[str] = None      # remote destination filename
#     chmod_x: bool = True
#     remote_cmd: Optional[str] = None       # the command to run in background
#     workdir: Optional[str] = None          # remote working directory (cd first)
#     env: Dict[str, str] = field(default_factory=dict)
#     pidfile: str = "/tmp/orch_bg.pid"
#     logfile: str = "/tmp/orch_bg.log"
#     stop_signal: str = "TERM"              # TERM then KILL fallback
#     stop_timeout: float = 5.0

#     # Host test
#     host_cmd: Optional[str] = None
#     host_timeout: Optional[float] = None   # seconds

#     # Expect
#     expect_host_contains: List[str] = field(default_factory=list)
#     expect_host_not_contains: List[str] = field(default_factory=list)
#     expect_host_regex: List[str] = field(default_factory=list)

#     expect_target_contains: List[str] = field(default_factory=list)
#     expect_target_not_contains: List[str] = field(default_factory=list)
#     expect_target_regex: List[str] = field(default_factory=list)

#     # Delays
#     post_start_delay: float = 0.5          # wait after starting remote bg before host test

#     def parse_target(self) -> Tuple[str, str]:
#         """Return (user, host) given target string."""
#         t = self.target.strip()
#         if "@" in t:
#             user, host = t.split("@", 1)
#         else:
#             user, host = "root", t
#         return user, host


# # ----------------------------- Text tidy helpers -----------------------------

# def _tidy_embedded_newlines(text: str) -> str:
#     """
#     Normalize/pretty-print outputs that contain *escaped* newline sequences.

#     Converts:
#       - Windows newlines to LF
#       - Literal backslash-escaped CRLF ('\\r\\n') to newline
#       - Literal backslash-escaped LF ('\\n') to newline
#       - Literal backslash-escaped CR ('\\r') to newline
#     Leaves other escape sequences alone to avoid over-decoding.
#     """
#     if not text:
#         return text
#     # Normalize real CRLF first
#     t = text.replace("\r\n", "\n")
#     # Then handle escaped sequences
#     t = t.replace("\\r\\n", "\n")
#     t = t.replace("\\n", "\n")
#     t = t.replace("\\r", "\n")
#     return t


# def _combine_and_tidy(host_out: str, host_err: str) -> Tuple[str, str, str]:
#     """Return (tidy_out, tidy_err, tidy_both_for_expect)."""
#     tidy_out = _tidy_embedded_newlines(host_out or "")
#     tidy_err = _tidy_embedded_newlines(host_err or "")
#     # For expectations, evaluate against combined output (stdout + stderr)
#     tidy_both = tidy_out + ("\n" + tidy_err if tidy_err else "")
#     return tidy_out, tidy_err, tidy_both


# def start_remote_background(t: SSHTarget, plan: Plan) -> Tuple[bool, str]:
#     """
#     Start remote background process with nohup & capture PID, write to pidfile and log.
#     Returns (ok, pid_str_or_error).
#     """
#     # Build environment prefix
#     env_prefix = ""
#     if plan.env:
#         env_prefix = " ".join([f'{shlex.quote(k)}={shlex.quote(v)}' for k, v in plan.env.items()]) + " "

#     # Optional cd
#     cd_prefix = ""
#     if plan.workdir:
#         cd_prefix = f"cd {shlex.quote(plan.workdir)} && "
#     log_prefix = ""
#     if plan.logfile:
#         log_prefix = f"rm -f {shlex.quote(plan.logfile)} && "

#     # Command to run
#     if not plan.remote_cmd:
#         return False, "remote_cmd is required to start background task"

#     # Compose the nohup launch; print PID to stdout so we can grab it
#     # Redirect stdin from /dev/null to avoid holding the channel open.
#     cmd = (
#         f"{cd_prefix}"
#         f"{log_prefix}"
#         f"sh -lc {shlex.quote(env_prefix + f'nohup {plan.remote_cmd} >{plan.logfile} 2>&1 < /dev/null & echo $!')}"
#     )

#     rc, out, err = t.run(cmd)
#     if rc != 0:
#         return False, f"remote start failed rc={rc}, err={err.strip()}"

#     pid = out.strip().splitlines()[-1].strip() if out.strip() else ""
#     if not pid.isdigit():
#         # Some shells may emit extra text; try last numeric token
#         toks = re.findall(r"\b(\d+)\b", out)
#         pid = toks[-1] if toks else ""

#     if not pid:
#         return False, f"could not parse PID from: {out!r} / {err!r}"

#     # Save pidfile
#     rc2, _, err2 = t.run(f"printf %s {shlex.quote(pid)} > {shlex.quote(plan.pidfile)}")
#     if rc2 != 0:
#         return False, f"failed to write pidfile: {err2.strip()}"

#     return True, pid


# def stop_remote_background(t: SSHTarget, plan: Plan) -> Tuple[bool, str]:
#     """
#     Stop remote background by reading pidfile and signaling the process.
#     TERM -> wait -> if still alive, KILL.
#     """
#     # Read PID
#     rc, out, err = t.run(f"cat {shlex.quote(plan.pidfile)} 2>/dev/null || true")
#     pid = (out or "").strip()
#     if not pid.isdigit():
#         return False, "no pidfile / invalid PID"

#     # Send TERM
#     rc, _, _ = t.run(f"kill -{plan.stop_signal} {pid} 2>/dev/null || true")
#     # Wait a bit and see if still alive
#     deadline = time.time() + plan.stop_timeout
#     while time.time() < deadline:
#         rc, out, _ = t.run(f"kill -0 {pid} 2>/dev/null; echo $?")
#         alive = (out.strip().splitlines()[-1].strip() == "0")
#         if not alive:
#             break
#         time.sleep(0.25)

#     # If still alive, KILL
#     rc, out, _ = t.run(f"kill -0 {pid} 2>/dev/null; echo $?")
#     alive = (out.strip().splitlines()[-1].strip() == "0")
#     if alive:
#         t.run(f"kill -KILL {pid} 2>/dev/null || true")

#     return True, pid


# def run_host_cmd(cmd: str, timeout: Optional[float]) -> Tuple[int, str, str]:
#     """
#     Run a local host command, capture rc/stdout/stderr.
#     (Raw capture; tidying happens in run_plan so expectations see prettified text.)
#     """
#     proc = subprocess.Popen(
#         cmd,
#         shell=True,
#         stdout=subprocess.PIPE,
#         stderr=subprocess.PIPE,
#         text=True,
#     )
#     try:
#         out, err = proc.communicate(timeout=timeout)
#         return proc.returncode, out, err
#     except subprocess.TimeoutExpired:
#         proc.kill()
#         out, err = proc.communicate()
#         return 124, out, (err or "") + "\nTIMEOUT"


# def run_plan(plan: Plan) -> Dict[str, object]:
#     # Connect SSH
#     user, host = plan.parse_target()
#     ssh = SSHTarget(
#         host=host,
#         user=user,
#         port=plan.port,
#         password=plan.password,
#         keyfile=plan.keyfile,
#         timeout=plan.timeout,
#     )

#     remote_started = False
#     remote_pid = ""
#     uploaded = None

#     try:
#         ssh.connect()

#         # 1) Optional upload
#         if plan.upload_src and plan.upload_dest:
#             with ssh.sftp() as s:
#                 s.put(plan.upload_src, plan.upload_dest)
#             uploaded = plan.upload_dest
#             if plan.chmod_x:
#                 ssh.run(f"chmod a+x {shlex.quote(plan.upload_dest)}")

#         # 2) Start remote BG
#         ok, info = start_remote_background(ssh, plan)
#         if not ok:
#             return {"ok": False, "stage": "start_remote", "error": info}
#         remote_started = True
#         remote_pid = info

#         # 3) Small wait to let it initialize
#         time.sleep(plan.post_start_delay)

#         # 4) Run host test (optional)
#         host_rc = None
#         host_out = ""
#         host_err = ""
#         host_expect = {}

#         if plan.host_cmd:
#             host_rc, host_out_raw, host_err_raw = run_host_cmd(plan.host_cmd, plan.host_timeout)
#             # Tidy embedded newlines in the host outputs so \n -> actual newlines
#             host_out, host_err, host_full_for_expect = _combine_and_tidy(host_out_raw, host_err_raw)
#             # Evaluate expectations on *tidied* host stdout+stderr
#             host_expect = expect_strings(
#                 host_full_for_expect,
#                 contains=plan.expect_host_contains,
#                 not_contains=plan.expect_host_not_contains,
#                 regex=plan.expect_host_regex,
#             )
#         else:
#             host_out = ""
#             host_err = ""
#             host_rc = 0
#             host_expect = {"pass": True, "failures": [], "length": 0}

#         # 5) Stop remote BG
#         stop_ok, stop_info = stop_remote_background(ssh, plan)

#         # 6) Fetch remote log for expectations
#         remote_log_text = ""
#         try:
#             with ssh.sftp() as s:
#                 with s.open(plan.logfile, "r") as fh:
#                     remote_log_text = fh.read().decode("utf-8", errors="replace")
#         except Exception:
#             # log may not exist; tolerate
#             pass

#         target_expect = expect_strings(
#             remote_log_text,
#             contains=plan.expect_target_contains,
#             not_contains=plan.expect_target_not_contains,
#             regex=plan.expect_target_regex,
#         )

#         # Overall pass: remote started + stopped + (if host_cmd) host_rc==0 + both expectations pass
#         overall_ok = True
#         if not remote_started:
#             overall_ok = False
#         if plan.host_cmd and (host_rc is None or host_rc != 0):
#             overall_ok = False
#         if host_expect.get("pass") is False:
#             overall_ok = False
#         if target_expect.get("pass") is False:
#             overall_ok = False

#         return {
#             "ok": overall_ok,
#             "remote": {
#                 "started": remote_started, "pid": remote_pid,
#                 "stopped": stop_ok, "stop_info": stop_info
#             },
#             "upload": {"dest": uploaded},
#             "host": {
#                 "rc": host_rc, "stdout": host_out, "stderr": host_err, "expect": host_expect
#             },
#             "target_log": {
#                 "path": plan.logfile, "length": len(remote_log_text), "sample": remote_log_text[-2048:]
#             },
#             "target_expect": target_expect,
#         }

#     except Exception as e:
#         return {"ok": False, "stage": "exception", "error": str(e)}
#     finally:
#         try:
#             ssh.close()
#         except Exception:
#             pass


# # ----------------------------- CLI -----------------------------

# def load_plan_from_file(path: str) -> Plan:
#     with open(path, "r", encoding="utf-8") as f:
#         j = json.load(f)

#     # Simple mapping with defaults, tolerating missing keys
#     def g(*ks, default=None):
#         d = j
#         for k in ks:
#             d = d.get(k, {})
#         return d if d else default

#     return Plan(
#         target = j.get("target") or "root@127.0.0.1",
#         port   = j.get("port", 22),
#         password = j.get("password"),
#         keyfile  = j.get("keyfile"),
#         timeout  = j.get("timeout", 10.0),

#         upload_src  = g("background", "upload", "src", default=j.get("upload_src")),
#         upload_dest = g("background", "upload", "dest", default=j.get("upload_dest")),
#         chmod_x     = g("background", "chmod_x", default=j.get("chmod_x", True)),
#         remote_cmd  = g("background", "cmd", default=j.get("remote_cmd")),
#         workdir     = g("background", "workdir", default=j.get("workdir")),
#         env         = g("background", "env", default=j.get("env", {})) or {},
#         pidfile     = g("background", "pidfile", default=j.get("pidfile", "/tmp/orch_bg.pid")),
#         logfile     = g("background", "logfile", default=j.get("logfile", "/tmp/orch_bg.log")),
#         stop_signal = g("background", "stop_signal", default=j.get("stop_signal", "TERM")),
#         stop_timeout= g("background", "stop_timeout", default=j.get("stop_timeout", 5.0)),

#         host_cmd    = g("host_test", "cmd", default=j.get("host_cmd")),
#         host_timeout= g("host_test", "timeout", default=j.get("host_timeout")),

#         expect_host_contains = g("expect", "host", "contains", default=j.get("expect_host_contains", [])) or [],
#         expect_host_not_contains = g("expect", "host", "not_contains", default=j.get("expect_host_not_contains", [])) or [],
#         expect_host_regex = g("expect", "host", "regex", default=j.get("expect_host_regex", [])) or [],

#         expect_target_contains = g("expect", "target", "contains", default=j.get("expect_target_contains", [])) or [],
#         expect_target_not_contains = g("expect", "target", "not_contains", default=j.get("expect_target_not_contains", [])) or [],
#         expect_target_regex = g("expect", "target", "regex", default=j.get("expect_target_regex", [])) or [],

#         post_start_delay = j.get("post_start_delay", 0.5),
#     )


# def main():
#     ap = argparse.ArgumentParser(description="Orchestrate remote BG task + host test + cleanup")
#     ap.add_argument("--config", default="orch/orc_plan.json", help="JSON plan file")
#     ap.add_argument("--target", help="user@host or host")
#     ap.add_argument("--port", type=int, default=22)
#     ap.add_argument("--password")
#     ap.add_argument("--keyfile")

#     ap.add_argument("--upload-src")
#     ap.add_argument("--upload-dest")
#     ap.add_argument("--no-chmod", action="store_true")

#     ap.add_argument("--remote-cmd", help="Command to run in background on target")
#     ap.add_argument("--workdir")
#     ap.add_argument("--pidfile", default="/tmp/orch_bg.pid")
#     ap.add_argument("--logfile", default="/tmp/orch_bg.log")

#     ap.add_argument("--host-cmd", help="Local host command to run")
#     ap.add_argument("--host-timeout", type=float)

#     ap.add_argument("--expect-host-contains", action="append", default=[])
#     ap.add_argument("--expect-host-not-contains", action="append", default=[])
#     ap.add_argument("--expect-host-regex", action="append", default=[])

#     ap.add_argument("--expect-target-contains", action="append", default=[])
#     ap.add_argument("--expect-target-not-contains", action="append", default=[])
#     ap.add_argument("--expect-target-regex", action="append", default=[])

#     ap.add_argument("--post-start-delay", type=float, default=0.5)

#     args = ap.parse_args()

#     if args.config:
#         plan = load_plan_from_file(args.config)
#     else:
#         if not args.target:
#             ap.error("--target or --config required")
#         plan = Plan(
#             target=args.target, port=args.port, password=args.password, keyfile=args.keyfile,
#             upload_src=args.upload_src, upload_dest=args.upload_dest, chmod_x=not args.no_chmod,
#             remote_cmd=args.remote_cmd, workdir=args.workdir, pidfile=args.pidfile, logfile=args.logfile,
#             host_cmd=args.host_cmd, host_timeout=args.host_timeout,
#             expect_host_contains=args.expect_host_contains,
#             expect_host_not_contains=args.expect_host_not_contains,
#             expect_host_regex=args.expect_host_regex,
#             expect_target_contains=args.expect_target_contains,
#             expect_target_not_contains=args.expect_target_not_contains,
#             expect_target_regex=args.expect_target_regex,
#             post_start_delay=args.post_start_delay,
#         )

#     # Sanity
#     if not plan.remote_cmd:
#         print("ERROR: remote_cmd is required (either in --remote-cmd or config.background.cmd)", file=sys.stderr)
#         sys.exit(2)

#     res = run_plan(plan)
#     # ensure_ascii=False keeps any non-ASCII characters readable
#     print(json.dumps(res, indent=2, ensure_ascii=False))
#     sys.exit(0 if res.get("ok") else 1)


# if __name__ == "__main__":
#     main()
# # # here is a single “orchestrator” that:

# # # 1. (optionally) pushes a program/script to the target via SSH/SFTP, `chmod +x`
# # # 2. starts it **in the background** on the target (PID + log captured)
# # # 3. runs a **host-side** test command (e.g., your CAN/WS test) and waits for it
# # # 4. then **stops** the background task on the target and (optionally) checks the target log
# # # 5. evaluates expectations (including “absence of a string”) and returns a JSON-like summary.

# # # Below is a self-contained script that does exactly that.

# # # > Deps: `pip install paramiko`
# # > Works with password or key auth. No `ssh root@ip:22 <<< …` tricks — just clean Paramiko.

# # ---

# # ### `orchestrate_target_test.py`

# # ```python
# # Orchestrate: upload -> start remote BG task -> run local host test -> stop remote -> evaluate
# #
# # Deps: pip install paramiko
# #
# # Usage (config file):
# #   python3 orchestrate_target_test.py --config test_plan.json
# #
# # Or ad-hoc CLI (no file upload, simple host cmd & expectations):
# #   python3 orchestrate_target_test.py \
# #     --target root@192.168.86.26 --port 22 --password 'mypw' \
# #     --remote-cmd '/tmp/mydaemon --arg foo' \
# #     --host-cmd 'python3 can_ws_soc_test_py.py' \
# #     --expect-host-contains 'PASS' --expect-host-not-contains 'FAIL' \
# #     --expect-target-not-contains 'panic' --expect-target-not-contains 'ERROR'

# import argparse
# import json
# import os
# import re
# import shlex
# import subprocess
# import sys
# import time
# from dataclasses import dataclass, field
# from typing import Dict, List, Optional, Tuple

# import paramiko


# # ----------------------------- Expect helpers -----------------------------

# def expect_strings(
#     text: str,
#     contains: Optional[List[str]] = None,
#     not_contains: Optional[List[str]] = None,
#     regex: Optional[List[str]] = None,
# ) -> Dict[str, object]:
#     """Evaluate simple expectations on a text blob."""
#     contains = contains or []
#     not_contains = not_contains or []
#     regex = regex or []

#     ok = True
#     failures = []

#     for s in contains:
#         if s not in text:
#             ok = False
#             failures.append(f"missing required substring: {s!r}")

#     for s in not_contains:
#         if s in text:
#             ok = False
#             failures.append(f"found forbidden substring: {s!r}")

#     for r in regex:
#         try:
#             if not re.search(r, text, flags=re.MULTILINE | re.DOTALL):
#                 ok = False
#                 failures.append(f"regex not matched: /{r}/")
#         except re.error as e:
#             ok = False
#             failures.append(f"bad regex /{r}/: {e}")

#     return {"pass": ok, "failures": failures, "length": len(text)}


# # ----------------------------- SSH helper -----------------------------

# @dataclass
# class SSHTarget:
#     host: str
#     user: str = "root"
#     port: int = 22
#     password: Optional[str] = None
#     keyfile: Optional[str] = None
#     timeout: float = 10.0

#     client: Optional[paramiko.SSHClient] = field(default=None, init=False)

#     def connect(self):
#         if self.client:
#             return
#         self.client = paramiko.SSHClient()
#         self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

#         kwargs = {
#             "hostname": self.host,
#             "port": self.port,
#             "username": self.user,
#             "timeout": self.timeout,
#             "allow_agent": True,
#             "look_for_keys": True,
#         }
#         if self.password:
#             kwargs["password"] = self.password
#         else:
#             kwargs["password"] = ""

#         if self.keyfile:
#             kwargs["key_filename"] = self.keyfile

#         self.client.connect(**kwargs)

#     def close(self):
#         if self.client:
#             try:
#                 self.client.close()
#             finally:
#                 self.client = None

#     def sftp(self):
#         assert self.client, "SSH not connected"
#         return self.client.open_sftp()

#     def run(self, cmd: str, get_pty: bool = False, timeout: Optional[float] = None) -> Tuple[int, str, str]:
#         """Run a command and return (rc, stdout, stderr)."""
#         assert self.client, "SSH not connected"
#         stdin, stdout, stderr = self.client.exec_command(cmd, get_pty=get_pty, timeout=timeout)
#         out = stdout.read().decode(errors="replace")
#         err = stderr.read().decode(errors="replace")
#         rc = stdout.channel.recv_exit_status()
#         return rc, out, err


# # ----------------------------- Orchestrator -----------------------------

# @dataclass
# class Plan:
#     # Target connection
#     target: str  # user@host or host
#     port: int = 22
#     password: Optional[str] = None
#     keyfile: Optional[str] = None
#     timeout: float = 10.0

#     # Background program (optional upload)
#     upload_src: Optional[str] = None       # local path to upload
#     upload_dest: Optional[str] = None      # remote destination filename
#     chmod_x: bool = True
#     remote_cmd: Optional[str] = None       # the command to run in background
#     workdir: Optional[str] = None          # remote working directory (cd first)
#     env: Dict[str, str] = field(default_factory=dict)
#     pidfile: str = "/tmp/orch_bg.pid"
#     logfile: str = "/tmp/orch_bg.log"
#     stop_signal: str = "TERM"              # TERM then KILL fallback
#     stop_timeout: float = 5.0

#     # Host test
#     host_cmd: Optional[str] = None
#     host_timeout: Optional[float] = None   # seconds

#     # Expect
#     expect_host_contains: List[str] = field(default_factory=list)
#     expect_host_not_contains: List[str] = field(default_factory=list)
#     expect_host_regex: List[str] = field(default_factory=list)

#     expect_target_contains: List[str] = field(default_factory=list)
#     expect_target_not_contains: List[str] = field(default_factory=list)
#     expect_target_regex: List[str] = field(default_factory=list)

#     # Delays
#     post_start_delay: float = 0.5          # wait after starting remote bg before host test

#     def parse_target(self) -> Tuple[str, str]:
#         """Return (user, host) given target string."""
#         t = self.target.strip()
#         if "@" in t:
#             user, host = t.split("@", 1)
#         else:
#             user, host = "root", t
#         return user, host


# def start_remote_background(t: SSHTarget, plan: Plan) -> Tuple[bool, str]:
#     """
#     Start remote background process with nohup & capture PID, write to pidfile and log.
#     Returns (ok, pid_str_or_error).
#     """
#     # Build environment prefix
#     env_prefix = ""
#     if plan.env:
#         env_prefix = " ".join([f'{shlex.quote(k)}={shlex.quote(v)}' for k, v in plan.env.items()]) + " "

#     # Optional cd
#     cd_prefix = ""
#     if plan.workdir:
#         cd_prefix = f"cd {shlex.quote(plan.workdir)} && "
#     log_prefix = ""
#     if plan.logfile:
#         log_prefix = f"rm -f {shlex.quote(plan.logfile)} && "

#     # Command to run
#     if not plan.remote_cmd:
#         return False, "remote_cmd is required to start background task"

#     # Compose the nohup launch; print PID to stdout so we can grab it
#     # Redirect stdin from /dev/null to avoid holding the channel open.
#     cmd = (
#         f"{cd_prefix}"
#         f"{log_prefix}"
#         f"sh -lc {shlex.quote(env_prefix + f'nohup {plan.remote_cmd} >{plan.logfile} 2>&1 < /dev/null & echo $!')}"
#     )

#     rc, out, err = t.run(cmd)
#     if rc != 0:
#         return False, f"remote start failed rc={rc}, err={err.strip()}"

#     pid = out.strip().splitlines()[-1].strip() if out.strip() else ""
#     if not pid.isdigit():
#         # Some shells may emit extra text; try last numeric token
#         toks = re.findall(r"\b(\d+)\b", out)
#         pid = toks[-1] if toks else ""

#     if not pid:
#         return False, f"could not parse PID from: {out!r} / {err!r}"

#     # Save pidfile
#     rc2, _, err2 = t.run(f"printf %s {shlex.quote(pid)} > {shlex.quote(plan.pidfile)}")
#     if rc2 != 0:
#         return False, f"failed to write pidfile: {err2.strip()}"

#     return True, pid


# def stop_remote_background(t: SSHTarget, plan: Plan) -> Tuple[bool, str]:
#     """
#     Stop remote background by reading pidfile and signaling the process.
#     TERM -> wait -> if still alive, KILL.
#     """
#     # Read PID
#     rc, out, err = t.run(f"cat {shlex.quote(plan.pidfile)} 2>/dev/null || true")
#     pid = (out or "").strip()
#     if not pid.isdigit():
#         return False, "no pidfile / invalid PID"

#     # Send TERM
#     rc, _, _ = t.run(f"kill -{plan.stop_signal} {pid} 2>/dev/null || true")
#     # Wait a bit and see if still alive
#     deadline = time.time() + plan.stop_timeout
#     while time.time() < deadline:
#         rc, out, _ = t.run(f"kill -0 {pid} 2>/dev/null; echo $?")
#         alive = (out.strip().splitlines()[-1].strip() == "0")
#         if not alive:
#             break
#         time.sleep(0.25)

#     # If still alive, KILL
#     rc, out, _ = t.run(f"kill -0 {pid} 2>/dev/null; echo $?")
#     alive = (out.strip().splitlines()[-1].strip() == "0")
#     if alive:
#         t.run(f"kill -KILL {pid} 2>/dev/null || true")

#     return True, pid


# def run_host_cmd(cmd: str, timeout: Optional[float]) -> Tuple[int, str, str]:
#     """
#     Run a local host command, capture rc/stdout/stderr.
#     """
#     proc = subprocess.Popen(
#         cmd,
#         shell=True,
#         stdout=subprocess.PIPE,
#         stderr=subprocess.PIPE,
#         text=True,
#     )
#     try:
#         out, err = proc.communicate(timeout=timeout)
#         return proc.returncode, out, err
#     except subprocess.TimeoutExpired:
#         proc.kill()
#         out, err = proc.communicate()
#         return 124, out, (err or "") + "\nTIMEOUT"


# def run_plan(plan: Plan) -> Dict[str, object]:
#     # Connect SSH
#     user, host = plan.parse_target()
#     ssh = SSHTarget(
#         host=host,
#         user=user,
#         port=plan.port,
#         password=plan.password,
#         keyfile=plan.keyfile,
#         timeout=plan.timeout,
#     )

#     remote_started = False
#     remote_pid = ""
#     uploaded = None

#     try:
#         ssh.connect()

#         # 1) Optional upload
#         if plan.upload_src and plan.upload_dest:
#             print(" upload src", plan.upload_src)
#             with ssh.sftp() as s:
#                 s.put(plan.upload_src, plan.upload_dest)
#             uploaded = plan.upload_dest
#             if plan.chmod_x:
#                 ssh.run(f"chmod a+x {shlex.quote(plan.upload_dest)}")

#         # 2) Start remote BG
#         ok, info = start_remote_background(ssh, plan)
#         if not ok:
#             return {"ok": False, "stage": "start_remote", "error": info}
#         remote_started = True
#         remote_pid = info

#         # 3) Small wait to let it initialize
#         time.sleep(plan.post_start_delay)

#         # 4) Run host test (optional)
#         host_rc = None
#         host_out = ""
#         host_err = ""
#         host_expect = {}

#         if plan.host_cmd:
#             host_rc, host_out, host_err = run_host_cmd(plan.host_cmd, plan.host_timeout)
#             # Evaluate expectations on host stdout+stderr
#             host_full = host_out + ("\n" + host_err if host_err else "")
#             host_expect = expect_strings(
#                 host_full,
#                 contains=plan.expect_host_contains,
#                 not_contains=plan.expect_host_not_contains,
#                 regex=plan.expect_host_regex,
#             )

#         # 5) Stop remote BG
#         stop_ok, stop_info = stop_remote_background(ssh, plan)

#         # 6) Fetch remote log for expectations
#         remote_log_text = ""
#         try:
#             with ssh.sftp() as s:
#                 with s.open(plan.logfile, "r") as fh:
#                     remote_log_text = fh.read().decode("utf-8", errors="replace")
#         except Exception:
#             # log may not exist; tolerate
#             pass

#         target_expect = expect_strings(
#             remote_log_text,
#             contains=plan.expect_target_contains,
#             not_contains=plan.expect_target_not_contains,
#             regex=plan.expect_target_regex,
#         )

#         # Overall pass: remote started + stopped + (if host_cmd) host_rc==0 + both expectations pass
#         overall_ok = True
#         if not remote_started:
#             overall_ok = False
#         if plan.host_cmd and (host_rc is None or host_rc != 0):
#             overall_ok = False
#         if host_expect.get("pass") is False:
#             overall_ok = False
#         if target_expect.get("pass") is False:
#             overall_ok = False

#         return {
#             "ok": overall_ok,
#             "remote": {
#                 "started": remote_started, "pid": remote_pid,
#                 "stopped": stop_ok, "stop_info": stop_info
#             },
#             "upload": {"dest": uploaded},
#             "host": {
#                 "rc": host_rc, "stdout": host_out, "stderr": host_err, "expect": host_expect
#             },
#             "target_log": {
#                 "path": plan.logfile, "length": len(remote_log_text), "sample": remote_log_text[-2048:]
#             },
#             "target_expect": target_expect,
#         }

#     except Exception as e:
#         return {"ok": False, "stage": "exception", "error": str(e)}
#     finally:
#         try: ssh.close()
#         except Exception: pass


# # ----------------------------- CLI -----------------------------

# def load_plan_from_file(path: str) -> Plan:
#     with open(path, "r", encoding="utf-8") as f:
#         j = json.load(f)

#     # Simple mapping with defaults, tolerating missing keys
#     def g(*ks, default=None):
#         d = j
#         for k in ks:
#             d = d.get(k, {})
#         return d if d else default

#     return Plan(
#         target = j.get("target") or "root@127.0.0.1",
#         port   = j.get("port", 22),
#         password = j.get("password"),
#         keyfile  = j.get("keyfile"),
#         timeout  = j.get("timeout", 10.0),

#         upload_src  = g("background", "upload", "src", default=j.get("upload_src")),
#         upload_dest = g("background", "upload", "dest", default=j.get("upload_dest")),
#         chmod_x     = g("background", "chmod_x", default=j.get("chmod_x", True)),
#         remote_cmd  = g("background", "cmd", default=j.get("remote_cmd")),
#         workdir     = g("background", "workdir", default=j.get("workdir")),
#         env         = g("background", "env", default=j.get("env", {})) or {},
#         pidfile     = g("background", "pidfile", default=j.get("pidfile", "/tmp/orch_bg.pid")),
#         logfile     = g("background", "logfile", default=j.get("logfile", "/tmp/orch_bg.log")),
#         stop_signal = g("background", "stop_signal", default=j.get("stop_signal", "TERM")),
#         stop_timeout= g("background", "stop_timeout", default=j.get("stop_timeout", 5.0)),

#         host_cmd    = g("host_test", "cmd", default=j.get("host_cmd")),
#         host_timeout= g("host_test", "timeout", default=j.get("host_timeout")),

#         expect_host_contains = g("expect", "host", "contains", default=j.get("expect_host_contains", [])) or [],
#         expect_host_not_contains = g("expect", "host", "not_contains", default=j.get("expect_host_not_contains", [])) or [],
#         expect_host_regex = g("expect", "host", "regex", default=j.get("expect_host_regex", [])) or [],

#         expect_target_contains = g("expect", "target", "contains", default=j.get("expect_target_contains", [])) or [],
#         expect_target_not_contains = g("expect", "target", "not_contains", default=j.get("expect_target_not_contains", [])) or [],
#         expect_target_regex = g("expect", "target", "regex", default=j.get("expect_target_regex", [])) or [],

#         post_start_delay = j.get("post_start_delay", 0.5),
#     )


# def main():
#     ap = argparse.ArgumentParser(description="Orchestrate remote BG task + host test + cleanup")
#     ap.add_argument("--config", default="orch/orc_plan.json", help="JSON plan file")
#     ap.add_argument("--target", help="user@host or host")
#     ap.add_argument("--port", type=int, default=22)
#     ap.add_argument("--password")
#     ap.add_argument("--keyfile")

#     ap.add_argument("--upload-src")
#     ap.add_argument("--upload-dest")
#     ap.add_argument("--no-chmod", action="store_true")

#     ap.add_argument("--remote-cmd", help="Command to run in background on target")
#     ap.add_argument("--workdir")
#     ap.add_argument("--pidfile", default="/tmp/orch_bg.pid")
#     ap.add_argument("--logfile", default="/tmp/orch_bg.log")

#     ap.add_argument("--host-cmd", help="Local host command to run")
#     ap.add_argument("--host-timeout", type=float)

#     ap.add_argument("--expect-host-contains", action="append", default=[])
#     ap.add_argument("--expect-host-not-contains", action="append", default=[])
#     ap.add_argument("--expect-host-regex", action="append", default=[])

#     ap.add_argument("--expect-target-contains", action="append", default=[])
#     ap.add_argument("--expect-target-not-contains", action="append", default=[])
#     ap.add_argument("--expect-target-regex", action="append", default=[])

#     ap.add_argument("--post-start-delay", type=float, default=0.5)

#     args = ap.parse_args()

#     if args.config:
#         plan = load_plan_from_file(args.config)
#     else:
#         if not args.target:
#             ap.error("--target or --config required")
#         plan = Plan(
#             target=args.target, port=args.port, password=args.password, keyfile=args.keyfile,
#             upload_src=args.upload_src, upload_dest=args.upload_dest, chmod_x=not args.no_chmod,
#             remote_cmd=args.remote_cmd, workdir=args.workdir, pidfile=args.pidfile, logfile=args.logfile,
#             host_cmd=args.host_cmd, host_timeout=args.host_timeout,
#             expect_host_contains=args.expect_host_contains,
#             expect_host_not_contains=args.expect_host_not_contains,
#             expect_host_regex=args.expect_host_regex,
#             expect_target_contains=args.expect_target_contains,
#             expect_target_not_contains=args.expect_target_not_contains,
#             expect_target_regex=args.expect_target_regex,
#             post_start_delay=args.post_start_delay,
#         )

#     # Sanity
#     if not plan.remote_cmd:
#         print("ERROR: remote_cmd is required (either in --remote-cmd or config.background.cmd)", file=sys.stderr)
#         sys.exit(2)
#     #print(" running plan")
#     res = run_plan(plan)
#     print(json.dumps(res, indent=2))
#     sys.exit(0 if res.get("ok") else 1)


# if __name__ == "__main__":
#     main()
# # ```

# # ---

# # ### Minimal JSON plan example

# # ```json
# # {
# #   "target": "root@192.168.86.26",
#   "port": 22,
#   "background": {
#     "upload": { "src": "./target_worker.sh", "dest": "/tmp/target_worker.sh" },
#     "chmod_x": true,
#     "cmd": "/tmp/target_worker.sh --listen 0.0.0.0:9009",
#     "workdir": "/tmp",
#     "env": { "FOO": "BAR" },
#     "pidfile": "/tmp/worker.pid",
#     "logfile": "/tmp/worker.log",
#     "stop_signal": "TERM",
#     "stop_timeout": 5.0
#   },
#   "host_test": {
#     "cmd": "python3 can_ws_soc_test_py.py",
#     "timeout": 120
#   },
#   "expect": {
#     "host": {
#       "contains": ["PASS"],
#       "not_contains": ["FAIL", "Traceback"]
#     },
#     "target": {
#       "not_contains": ["panic", "ERROR"]
#     }
#   },
#   "post_start_delay": 0.5
# }
# # ```

# ---

# ### Why this fixes your earlier SSH pain

# * That `ssh root@192.168.86.26:22 <<<` fails because OpenSSH doesn’t accept `:22` right after host in that position; port must be `-p 22`. Here we avoid shell quirks entirely and use Paramiko to do SSH/SFTP cleanly.
# * The remote background is started with `nohup … & echo $!` and we store the PID in a pidfile. We kill gracefully (TERM), wait, then KILL if needed.
# * Expectations support both presence and **absence** of strings (plus optional regex).
# * All outputs (host stdout/stderr and a tail of the remote log) are returned in one JSON summary so you can feed it back into your web UI later.

# If you want, I can add:

# * SCP fallback (using `subprocess ssh/scp`) if Paramiko isn’t available,
# * systemd-run or `start-stop-daemon` launch modes for distros that prefer them,
# * a simple REST wrapper so this exact flow can be triggered from your existing front end.
