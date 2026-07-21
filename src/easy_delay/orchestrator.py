from __future__ import annotations

from contextlib import suppress
from importlib.resources import files
import json
from pathlib import Path
import secrets
import shlex
import socket
import subprocess

import paramiko

from .config import Config, Target


ARCHITECTURE_NAMES = {
    "x86_64": "amd64",
    "amd64": "amd64",
    "aarch64": "arm64",
    "arm64": "arm64",
    "armv7l": "armv7",
    "armv7": "armv7",
}


def _connect(target: Target) -> paramiko.SSHClient:
    connection = paramiko.SSHClient()
    connection.load_system_host_keys()
    if target.host_key_policy == "auto_add":
        connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        connection.connect(
            hostname=target.host,
            port=target.ssh_port,
            username=target.user,
            password=target.password,
            timeout=10,
            auth_timeout=10,
            banner_timeout=10,
            allow_agent=False,
            look_for_keys=False,
        )
    except (OSError, paramiko.SSHException) as error:
        connection.close()
        raise RuntimeError(f"SSH connection failed: {error}") from error
    return connection


def _exec_checked(connection: paramiko.SSHClient, command: str) -> str:
    _, stdout, stderr = connection.exec_command(command, timeout=10)
    status = stdout.channel.recv_exit_status()
    output = stdout.read().decode().strip()
    if status != 0:
        message = stderr.read().decode().strip()
        raise RuntimeError(f"remote command failed ({status}): {message}")
    return output


def _detect_architecture(connection: paramiko.SSHClient) -> str:
    machine = _exec_checked(connection, "uname -m").lower()
    try:
        return ARCHITECTURE_NAMES[machine]
    except KeyError as error:
        raise RuntimeError(f"unsupported target architecture: {machine}") from error


def _server_binary(architecture: str) -> Path:
    binary_directory = Path(str(files("easy_delay").joinpath("bin")))
    cross_binary = binary_directory / f"easy-delay-server-{architecture}"
    if cross_binary.is_file():
        return cross_binary
    raise RuntimeError(
        f"wheel does not contain a server for {architecture}; "
        "place its static binary in prebuilt/ before building the wheel"
    )


def _run_client(config: Config, client: Path, session: str) -> dict[str, object]:
    command = [
        str(client), config.target.host, str(config.target.port), session,
        str(config.measurement.samples), str(config.measurement.interval_ms),
    ]
    result = subprocess.run(command, check=True, text=True, capture_output=True)
    return json.loads(result.stdout)


def _apply_verdict(config: Config, report: dict[str, object], architecture: str) -> None:
    offset = abs(float(report["offset_ms"]))
    conservative_offset = offset + float(report["minimum_rtt_ms"]) / 2.0
    threshold = config.measurement.threshold_ms
    margin = config.measurement.safety_margin_ms
    if conservative_offset < threshold - margin:
        verdict = "pass"
    elif conservative_offset > threshold + margin:
        verdict = "fail"
    else:
        verdict = "uncertain"
    report.update({
        "result": verdict,
        "target": config.target.host,
        "target_architecture": architecture,
        "threshold_ms": threshold,
        "conservative_offset_ms": conservative_offset,
    })


def measure(config: Config) -> dict[str, object]:
    target = config.target
    session = secrets.token_hex(8)
    connection = _connect(target)
    server_channel: paramiko.Channel | None = None
    remote_directory = f"{target.remote_directory}-{session}"
    remote_server = f"{remote_directory}/server"
    quoted_directory = shlex.quote(remote_directory)
    quoted_server = shlex.quote(remote_server)

    try:
        # Stage 1: Detect the target and select a libc-independent server artifact.
        architecture = _detect_architecture(connection)
        server = _server_binary(architecture)
        client = Path(str(files("easy_delay").joinpath("bin/easy-delay-client")))

        # Stage 2: Upload over the authenticated SSH connection.
        _exec_checked(connection, f"mkdir -p -- {quoted_directory}")
        with connection.open_sftp() as sftp:
            sftp.put(str(server), remote_server)
            sftp.chmod(remote_server, 0o700)

        # Stage 3: Start one temporary UDP server and wait for readiness.
        remote_command = f"exec {quoted_server} {target.port} {session}"
        _, server_stdout, server_stderr = connection.exec_command(remote_command)
        server_channel = server_stdout.channel
        server_channel.settimeout(10)
        try:
            ready_line = server_stdout.readline().strip()
        except socket.timeout as error:
            raise RuntimeError("remote server readiness timed out after 10 seconds") from error
        if ready_line != "READY":
            error = server_stderr.readline().strip()
            raise RuntimeError(f"remote server failed to start: {error or ready_line}")

        # Stage 4: Keep all timed operations and statistics inside the C++ client.
        report = _run_client(config, client, session)
        _apply_verdict(config, report, architecture)
        return report
    finally:
        # Stage 5: Close the channel and remove the unique remote temporary directory.
        if server_channel is not None:
            server_channel.close()
        with suppress(OSError, paramiko.SSHException, RuntimeError):
            _exec_checked(connection, f"rm -f -- {quoted_server}; rmdir -- {quoted_directory}")
        connection.close()
