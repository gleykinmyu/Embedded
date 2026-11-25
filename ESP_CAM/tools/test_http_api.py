#!/usr/bin/env python3
"""Интеграционные HTTP-проверки текущего этапа прошивки.

Реализованы challenge, login, logout, пароль, session TTL и Service token.
Прочие операции после успешного доступа по-прежнему 500 internal / not implemented.
Скрипт не шлёт команды движения, камеры, сети и raw-кадры.

Запуск:

  set ESP_CAM_PASSWORD=...
  set ESP_CAM_ALLOW_LOGIN=1
  python tools/test_http_api.py --host 192.168.0.254

Пароль и готовая cookie берутся только из окружения. Service token в файлы
и журнал не пишется: если ESP_CAM_TOKEN пуст, скрипт читает его через
GET /api/auth/token и держит только в памяти процесса.

  ESP_CAM_PASSWORD         пароль оператора, только для proof
  ESP_CAM_USER             имя, по умолчанию admin
  ESP_CAM_TOKEN            Bearer, если уже известен
  ESP_CAM_COOKIE           значение cookie esp_sess, если сессия уже открыта
  ESP_CAM_ALLOW_LOGIN      1 — challenge, login и logout тестовой сессии
  ESP_CAM_ALLOW_AUTH_WRITE 1 — смена пароля, TTL и перевыпуск token

Challenge сменяет общий nonce. Успешный login заменяет operator session
и может остановить оси. Logout завершает тестовую сессию. Без
ESP_CAM_ALLOW_LOGIN=1 эти проверки пропускаются.

Смена пароля и TTL восстанавливает прежние значения. Перевыпуск token
необратим: прежний Bearer перестаёт действовать. Эти три проверки
пропускаются без ESP_CAM_ALLOW_AUTH_WRITE=1.

Долгое ожидание истечения idle: --expire-session.
Проверка nonce старше 60 с: --expire-nonce.
Сохранение пароля, TTL и Service token через обновление прошивки и
физический перезапуск, одним процессом и без смены сети: --after-restart.
Token и пароль остаются в памяти процесса и в журнал не попадают.

Content-Length без тела ESP-IDF держит в разборе, пока не придёт первый
байт, и отвечает штатным 408 после recv_wait_timeout (5 с). Эта проверка
ждёт дольше сервера. Остальные проверки кадра идут отдельными соединениями.
"""

from __future__ import annotations

import argparse
import hashlib
import http.client
import json
import os
import re
import socket
import sys
import threading
import time
import urllib.parse
from dataclasses import dataclass, field


TIMEOUT_S = 5.0
# HTTPD_DEFAULT_CONFIG().recv_wait_timeout. Пустой кадр получает 408
# только после этого ожидания, поэтому клиент ждёт с запасом.
SERVER_RECV_TIMEOUT_S = 5.0
EMPTY_BODY_TIMEOUT_S = SERVER_RECV_TIMEOUT_S + 7.0
UI = "ui"
BAD_COOKIE = "invalid-session-cookie"
BAD_BEARER = "invalidtokeninvalidtokeninvalid"
ALLOW_REASON = (
    "нужно ESP_CAM_ALLOW_LOGIN=1: challenge сменяет общий nonce, "
    "успешный login заменяет operator session и может остановить оси, "
    "logout завершает тестовую сессию"
)
WRITE_REASON = (
    "нужно ESP_CAM_ALLOW_AUTH_WRITE=1: меняет пароль, TTL или Service token"
)
HEX32 = re.compile(r"^[0-9a-fA-F]{32}$")
TOKEN_RE = re.compile(r"^[A-Za-z0-9]{32}$")
PROOF_FILL = "ab" * 32


def allow_login() -> bool:
    return os.environ.get("ESP_CAM_ALLOW_LOGIN", "").strip() == "1"


def allow_auth_write() -> bool:
    return os.environ.get("ESP_CAM_ALLOW_AUTH_WRITE", "").strip() == "1"


@dataclass
class Stats:
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    failures: list[str] = field(default_factory=list)

    def ok(self, name: str) -> None:
        self.passed += 1
        print(f"PASS  {name}", flush=True)

    def fail(self, name: str, detail: str) -> None:
        self.failed += 1
        self.failures.append(f"{name}: {detail}")
        print(f"FAIL  {name}  {detail}", flush=True)

    def skip(self, name: str, reason: str) -> None:
        self.skipped += 1
        print(f"SKIP  {name}  {reason}", flush=True)


@dataclass
class Reply:
    status: int
    content_type: str
    body: bytes
    json_body: object | None
    error: str = ""
    set_cookie: str = ""
    cache_control: str = ""


class Client:
    def __init__(self, host: str, port: int, timeout: float) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self.cookie = os.environ.get("ESP_CAM_COOKIE", "").strip()
        self.token = os.environ.get("ESP_CAM_TOKEN", "").strip()
        self.user = os.environ.get("ESP_CAM_USER", "admin").strip() or "admin"

    def request(
        self,
        method: str,
        path: str,
        *,
        headers: dict[str, str] | None = None,
        body: bytes | None = None,
    ) -> Reply:
        hdrs = dict(headers or {})
        conn = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        try:
            conn.request(method, path, body=body, headers=hdrs)
            resp = conn.getresponse()
            raw = resp.read()
            ctype = resp.getheader("Content-Type") or ""
            set_cookie = resp.getheader("Set-Cookie") or ""
            cache_control = resp.getheader("Cache-Control") or ""
        except (OSError, http.client.HTTPException) as exc:
            return Reply(0, "", b"", None, str(exc))
        finally:
            conn.close()
        parsed: object | None = None
        if raw:
            try:
                parsed = json.loads(raw.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                parsed = None
        return Reply(
            resp.status,
            ctype,
            raw,
            parsed,
            set_cookie=set_cookie,
            cache_control=cache_control,
        )

    def legacy_get(self, path: str, headers: dict[str, str] | None = None) -> Reply:
        return self.request("GET", path, headers=headers)

    def open_session(self, stats: Stats) -> None:
        if self.cookie:
            print("cookie взята из ESP_CAM_COOKIE, новый login не выполняется")
        elif os.environ.get("ESP_CAM_PASSWORD", "") and not allow_login():
            print("ESP_CAM_PASSWORD задан, login не выполняется без ESP_CAM_ALLOW_LOGIN=1")
        elif not os.environ.get("ESP_CAM_PASSWORD", ""):
            print("ESP_CAM_PASSWORD и ESP_CAM_COOKIE не заданы: operator session недоступна")

    def headers(self, mode: str) -> dict[str, str] | None:
        """None means a required credential is absent and the test must SKIP."""
        ui = {"X-Esp-Client": UI}
        cookie = {"Cookie": f"esp_sess={self.cookie}"} if self.cookie else None
        bad_cookie = {"Cookie": f"esp_sess={BAD_COOKIE}"}
        bearer = {"Authorization": f"Bearer {self.token}"} if self.token else None
        bad_bearer = {"Authorization": f"Bearer {BAD_BEARER}"}
        table: dict[str, list[dict[str, str] | None]] = {
            "none": [{}],
            "ui": [ui],
            "cookie": [cookie],
            "cookie-ui": [cookie, ui],
            "bad-cookie-ui": [bad_cookie, ui],
            "bearer": [bearer],
            "bad-bearer": [bad_bearer],
            "cookie-bearer": [cookie, ui, bearer],
            "cookie-bad-bearer": [cookie, ui, bad_bearer],
            "bad-cookie-bearer": [bad_cookie, ui, bearer],
        }
        parts = table[mode]
        if any(part is None for part in parts):
            return None
        merged: dict[str, str] = {}
        for part in parts:
            assert part is not None
            merged.update(part)
        return merged


def snippet(reply: Reply) -> str:
    if reply.error:
        return reply.error
    text = reply.body.decode("utf-8", "replace")
    text = text.replace("\n", " ")
    if len(text) > 180:
        text = text[:180] + "..."
    return f"HTTP {reply.status} {text}"


def expect_error(
    stats: Stats,
    name: str,
    reply: Reply,
    status: int,
    code: str,
    message: str | None = None,
) -> None:
    if reply.error or reply.status == 0:
        stats.fail(name, snippet(reply))
        return
    problems: list[str] = []
    if reply.status != status:
        problems.append(f"status {reply.status} вместо {status}")
    if "application/json" not in reply.content_type.lower():
        problems.append(f"Content-Type {reply.content_type or 'пусто'}")
    body = reply.json_body if isinstance(reply.json_body, dict) else None
    if body is None:
        problems.append("тело не JSON-объект")
    else:
        if body.get("ok") is not False:
            problems.append("ok не false")
        err = body.get("error")
        if not isinstance(err, dict):
            problems.append("нет error")
        else:
            if err.get("code") != code:
                problems.append(f"code {err.get('code')!r} вместо {code}")
            if message is not None and err.get("message") != message:
                problems.append(f"message {err.get('message')!r}")
    if problems:
        stats.fail(name, "; ".join(problems) + " | " + snippet(reply))
    else:
        stats.ok(name)


def expect_challenge(stats: Stats, name: str, reply: Reply) -> None:
    if reply.error or reply.status == 0:
        stats.fail(name, snippet(reply))
        return
    problems: list[str] = []
    if reply.status != 200:
        problems.append(f"status {reply.status}")
    if "application/json" not in reply.content_type.lower():
        problems.append(f"Content-Type {reply.content_type or 'пусто'}")
    if reply.set_cookie:
        problems.append("challenge прислал Set-Cookie")
    body = reply.json_body if isinstance(reply.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    if not isinstance(body, dict) or body.get("ok") is not True or not isinstance(data, dict):
        problems.append("нет конверта ok/data")
    else:
        for key in ("salt", "nonce", "next_salt"):
            value = data.get(key)
            if not isinstance(value, str) or HEX32.fullmatch(value) is None:
                problems.append(f"{key} не 32 hex")
    if problems:
        stats.fail(name, "; ".join(problems))
    else:
        stats.ok(name)


def expect_probe(stats: Stats, name: str, reply: Reply, method: str) -> None:
    if reply.error or reply.status == 0:
        stats.fail(name, snippet(reply))
        return
    problems: list[str] = []
    if reply.status != 200:
        problems.append(f"status {reply.status}")
    if "application/json" not in reply.content_type.lower():
        problems.append(f"Content-Type {reply.content_type or 'пусто'}")
    body = reply.json_body if isinstance(reply.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    if not isinstance(body, dict) or body.get("ok") is not True or not isinstance(data, dict):
        problems.append("нет конверта ok/data")
    elif data.get("authenticated") is not (method != "none") or data.get("method") != method:
        problems.append(
            f"method={data.get('method')!r} authenticated={data.get('authenticated')!r}"
        )
    if problems:
        stats.fail(name, "; ".join(problems))
    else:
        stats.ok(name)


def seconds_left(reply: Reply) -> int | None:
    body = reply.json_body if isinstance(reply.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    if not isinstance(data, dict) or data.get("method") != "operator":
        return None
    value = data.get("seconds_left")
    return value if isinstance(value, int) else None


def run_routes(client: Client, stats: Stats) -> None:
    anon = client.headers("none") or {}
    cases = [
        ("неизвестный точный URI", "GET", "/api/does-not-exist", 404, "not_found", "not found"),
        ("неизвестный хвост GET", "GET", "/api/motion/no-such", 404, "not_found", "not found"),
        ("неизвестный хвост POST", "POST", "/api/motion/no-such", 404, "not_found", "not found"),
        ("неизвестный хвост PUT", "PUT", "/api/motion/no-such", 404, "not_found", "not found"),
        ("неизвестный хвост PATCH", "PATCH", "/api/motion/no-such", 404, "not_found", "not found"),
        ("неизвестный хвост DELETE", "DELETE", "/api/motion/no-such", 404, "not_found", "not found"),
        ("POST /api/status", "POST", "/api/status", 405, "invalid_parameter", "method not allowed"),
        ("точный POST /api/motion чужим GET", "GET", "/api/motion", 405, "invalid_parameter", "method not allowed"),
        ("известный stop чужим GET", "GET", "/api/motion/stop", 405, "invalid_parameter", "method not allowed"),
        ("известный preset чужим GET", "GET", "/api/presets/1", 405, "invalid_parameter", "method not allowed"),
        ("известный fan чужим POST", "POST", "/api/fans/primary", 405, "invalid_parameter", "method not allowed"),
        ("точный /api/aim чужим PATCH", "PATCH", "/api/aim", 405, "invalid_parameter", "method not allowed"),
        ("preset 0", "PUT", "/api/presets/0", 404, "not_found", "not found"),
        ("preset 100", "PUT", "/api/presets/100", 404, "not_found", "not found"),
        ("preset с ведущим нулём", "PUT", "/api/presets/01", 404, "not_found", "not found"),
        ("хвост preset не recall", "POST", "/api/presets/1/nope", 404, "not_found", "not found"),
        ("неизвестная ось лимита", "PUT", "/api/limits/pan", 404, "not_found", "not found"),
        ("неизвестный вентилятор", "PUT", "/api/fans/tertiary", 404, "not_found", "not found"),
        ("OSD имя вне шаблона", "GET", "/api/camera/osd/Bad", 404, "not_found", "not found"),
        ("wildcard values с query без доступа", "GET", "/api/camera/values?scope=full", 401, "unauthorized", None),
        ("probe с query", "GET", "/api/auth/session?unused=1", 200, None, None),
        ("точный /api/motion без доступа", "POST", "/api/motion", 401, "unauthorized", None),
        ("wildcard /api/motion/stop без доступа", "POST", "/api/motion/stop", 401, "unauthorized", None),
        ("точный /api/optics без доступа", "GET", "/api/optics", 401, "unauthorized", None),
        ("wildcard position-control без доступа", "PUT", "/api/optics/position-control", 401, "unauthorized", None),
        ("существующий preset без доступа", "PUT", "/api/presets/1", 401, "unauthorized", None),
        ("существующий лимит без доступа", "PUT", "/api/limits/tilt_up", 401, "unauthorized", None),
        ("существующий fan без доступа", "PUT", "/api/fans/primary", 401, "unauthorized", None),
        ("OSD по шаблону без доступа", "GET", "/api/camera/osd/good_name", 401, "unauthorized", None),
        ("известный osd/catalog без доступа", "GET", "/api/camera/osd/catalog", 401, "unauthorized", None),
    ]
    for name, method, path, status, code, message in cases:
        reply = client.request(method, path, headers=anon)
        if status == 200 and code is None:
            expect_probe(stats, name, reply, "none")
        else:
            assert code is not None
            expect_error(stats, name, reply, status, code, message)


def run_known_stub(client: Client, stats: Stats) -> None:
    authed = client.headers("bearer") or client.headers("cookie-ui")
    if authed is None:
        stats.skip("заглушка известного пути", "нет cookie и Bearer")
        return
    stub = client.request("GET", "/api/camera/values?scope=dependencies", headers=authed)
    expect_error(stats, "wildcard с query после доступа", stub, 500, "internal", "not implemented")


def mode_phase(mode: str) -> str:
    needs_bearer = mode in ("bearer", "cookie-bearer", "bad-cookie-bearer")
    if needs_bearer:
        return "bearer"
    if "cookie" in mode and "bad-cookie" not in mode:
        return "cookie"
    return "plain"


def run_auth_matrix(client: Client, stats: Stats, phase: str) -> None:
    public = ("POST", "/api/auth/challenge")
    operator = ("GET", "/api/auth/token")
    common = ("GET", "/api/aim")
    probe = ("GET", "/api/auth/session")
    # Public status/code, Operator status/code, Common status/code, probe method.
    spec = [
        ("none", 401, "unauthorized", 401, "unauthorized", 401, "unauthorized", "none"),
        ("cookie", 401, "unauthorized", 401, "unauthorized", 401, "unauthorized", "none"),
        ("cookie-ui", 500, "internal", 200, "token", 500, "internal", "operator"),
        ("bad-cookie-ui", 500, "internal", 401, "unauthorized", 401, "unauthorized", "none"),
        ("bearer", 401, "unauthorized", 403, "forbidden", 500, "internal", "service"),
        ("bad-bearer", 401, "unauthorized", 401, "unauthorized", 401, "unauthorized", "none"),
        ("cookie-bearer", 500, "internal", 200, "token", 500, "internal", "operator"),
        ("cookie-bad-bearer", 500, "internal", 200, "token", 401, "unauthorized", "operator"),
        ("bad-cookie-bearer", 500, "internal", 403, "forbidden", 500, "internal", "service"),
    ]
    labels = {
        "none": "нет credentials",
        "cookie": "cookie без X-Esp-Client",
        "cookie-ui": "валидная cookie",
        "bad-cookie-ui": "невалидная cookie",
        "bearer": "валидный Bearer",
        "bad-bearer": "невалидный Bearer",
        "cookie-bearer": "cookie и Bearer",
        "cookie-bad-bearer": "валидная cookie и неверный Bearer",
        "bad-cookie-bearer": "невалидная cookie и валидный Bearer",
    }
    rows: list[tuple[str, str, str, str, int, str | None, str | None]] = []
    for mode, pub_st, pub_code, op_st, op_code, com_st, com_code, who in spec:
        rows.append((mode, "Public", public[0], public[1], pub_st, pub_code, None))
        rows.append((mode, "Operator", operator[0], operator[1], op_st, op_code, None))
        rows.append((mode, "Common", common[0], common[1], com_st, com_code, None))
        rows.append((mode, "Probe", probe[0], probe[1], 200, None, who))

    public_issues = {
        "cookie-ui",
        "bad-cookie-ui",
        "cookie-bearer",
        "cookie-bad-bearer",
        "bad-cookie-bearer",
    }
    for mode, kind, method, path, status, code, who in rows:
        if mode_phase(mode) != phase:
            continue
        name = f"{kind}: {labels[mode]}"
        if kind == "Public" and mode in public_issues:
            if not allow_login():
                stats.skip(name, ALLOW_REASON)
                continue
            hdrs = client.headers(mode)
            if hdrs is None:
                missing = []
                if "cookie" in mode and "bad-cookie" not in mode and not client.cookie:
                    missing.append("operator cookie")
                if mode in ("cookie-bearer", "bad-cookie-bearer") and not client.token:
                    missing.append("Bearer")
                stats.skip(name, "нет " + " и ".join(missing or ["credentials"]))
                continue
            expect_challenge(stats, name, client.request(method, path, headers=hdrs))
            continue
        hdrs = client.headers(mode)
        if hdrs is None:
            missing = []
            if "cookie" in mode and "bad-cookie" not in mode and not client.cookie:
                missing.append("operator cookie")
            if mode in ("bearer", "cookie-bearer", "bad-cookie-bearer") and not client.token:
                missing.append("Bearer")
            stats.skip(name, "нет " + " и ".join(missing or ["credentials"]))
            continue
        reply = client.request(method, path, headers=hdrs)
        if kind == "Operator" and code == "token":
            token = expect_token(stats, name, reply)
            if token and not client.token:
                client.token = token
            continue
        if kind == "Probe":
            assert who is not None
            expect_probe(stats, name, reply, who)
        else:
            assert code is not None
            message = "not implemented" if code == "internal" else None
            expect_error(stats, name, reply, status, code, message)

    if phase != "bearer":
        return
    raw_headers = client.headers("bearer")
    raw_name = "Common: Bearer и raw при политике operator"
    if raw_headers is None:
        stats.skip(raw_name, "нет Bearer")
    else:
        reply = client.request("POST", "/api/diagnostics/protocol/head", headers=raw_headers)
        expect_error(stats, raw_name, reply, 403, "protocol_disabled", "protocol disabled")


def expect_token(stats: Stats, name: str, reply: Reply) -> str:
    """Проверяет TokenResult и не печатает сам token."""
    body = reply.json_body if isinstance(reply.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    token = data.get("token") if isinstance(data, dict) else None
    ok = (
        not reply.error
        and reply.status == 200
        and isinstance(body, dict)
        and body.get("ok") is True
        and isinstance(token, str)
        and TOKEN_RE.fullmatch(token) is not None
        and not reply.set_cookie
    )
    if ok:
        stats.ok(name)
        assert isinstance(token, str)
        return token
    stats.fail(name, f"HTTP {reply.status} нет TokenResult")
    return ""


HEAD_CAP_KEYS = (
    "absolute_aim",
    "absolute_aim_speed",
    "preset_map",
    "preset_options",
    "limit_set",
    "limit_toggle",
    "tally",
    "fan",
    "wiper",
    "washer",
    "wireless",
    "option",
    "tilt_span",
    "heater",
    "defroster",
    "aim_control",
    "aim_displayed",
    "optics",
    "exposure_reading",
    "landing",
    "head_version",
    "power_camera_link",
)
CAM_CAP_KEYS = (
    "hd_detail",
    "negative",
    "scan",
    "chroma",
    "black_shade",
    "synchro",
    "black_balance",
    "paint",
    "genlock",
    "gamma",
    "agc_ceiling",
    "master_detail",
    "monitor",
    "lens_interface",
    "menu_left_right",
)
HEAD_MODELS = {"ph350", "ph360", "ph650", "ph405", "he130", "he870", "ue150", "iptz"}
CAM_MODELS = {"e650", "e600", "he130", "he870", "ue150"}
POWER_CODE = {
    "off": 0,
    "on": 1,
    "on_camera_tx": 2,
    "on_no_camera_tx": 3,
    "starting": 14,
    "control_off": 15,
}
IPV4_RE = re.compile(r"^(?:\d{1,3}\.){3}\d{1,3}$")
MAC_RE = re.compile(r"^(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")


def status_problems(data: object, service: bool) -> list[str]:
    problems: list[str] = []
    if not isinstance(data, dict):
        return ["data не объект"]
    expect = {
        "network",
        "uptime_seconds",
        "session",
        "head",
        "camera",
        "motion",
        "presets",
        "limits",
    }
    if set(data) != expect:
        problems.append("состав BridgeStatus")
        return problems

    def need_obj(name: str) -> dict | None:
        value = data.get(name)
        if not isinstance(value, dict):
            problems.append(f"{name} не объект")
            return None
        return value

    net = need_obj("network")
    if net is not None:
        if set(net) != {"link", "address", "mask", "gateway", "mac"}:
            problems.append("состав network")
        elif not isinstance(net.get("link"), bool):
            problems.append("link")
        elif any(IPV4_RE.fullmatch(str(net.get(key))) is None for key in ("address", "mask", "gateway")):
            problems.append("ipv4")
        elif MAC_RE.fullmatch(str(net.get("mac"))) is None:
            problems.append("mac")
    uptime = data.get("uptime_seconds")
    if isinstance(uptime, bool) or not isinstance(uptime, int) or uptime < 0:
        problems.append("uptime_seconds")
    session = need_obj("session")
    if session is not None:
        if set(session) != {"ttl_seconds", "seconds_left"}:
            problems.append("состав session")
        ttl = session.get("ttl_seconds")
        left = session.get("seconds_left")
        if isinstance(ttl, bool) or not isinstance(ttl, int) or not 30 <= ttl <= 300:
            problems.append("ttl_seconds")
        elif service:
            if left is not None:
                problems.append("seconds_left service")
        elif isinstance(left, bool) or not isinstance(left, int) or left < 0 or (isinstance(ttl, int) and left > ttl):
            problems.append("seconds_left")
    head = need_obj("head")
    if head is not None:
        if set(head) != {
            "model",
            "name",
            "alive",
            "power",
            "powered",
            "optics_alive",
            "optics_fail_count",
            "fail_count",
            "error_code",
            "capabilities",
        }:
            problems.append("состав head")
        if head.get("model") not in HEAD_MODELS:
            problems.append("head.model")
        if not isinstance(head.get("name"), str) or not head.get("name"):
            problems.append("head.name")
        for key in ("alive", "powered", "optics_alive"):
            if not isinstance(head.get(key), bool):
                problems.append(key)
        for key in ("optics_fail_count", "fail_count"):
            value = head.get(key)
            if isinstance(value, bool) or not isinstance(value, int) or value < 0:
                problems.append(key)
        if head.get("error_code") is not None:
            problems.append("error_code")
        power = head.get("power")
        if not isinstance(power, dict) or set(power) != {"state", "label", "code"}:
            problems.append("power")
        else:
            state = power.get("state")
            code = power.get("code")
            label = power.get("label")
            if not isinstance(label, str) or not label:
                problems.append("power.label")
            if state not in POWER_CODE and state != "unknown":
                problems.append("power.state")
            elif state == "unknown":
                if code is not None and (isinstance(code, bool) or not isinstance(code, int) or not 0 <= code <= 255):
                    problems.append("power.code")
                if isinstance(code, int) and code in POWER_CODE.values():
                    problems.append("power.code известен при unknown")
            elif code != POWER_CODE[state]:
                problems.append("power.code")
        caps = head.get("capabilities")
        if not isinstance(caps, dict) or set(caps) != set(HEAD_CAP_KEYS):
            problems.append("head.capabilities")
        elif any(not isinstance(caps.get(key), bool) for key in HEAD_CAP_KEYS):
            problems.append("head.capabilities тип")
    camera = need_obj("camera")
    if camera is not None:
        if set(camera) != {
            "model",
            "name",
            "transmitter",
            "alive",
            "identifier",
            "fail_count",
            "capabilities",
        }:
            problems.append("состав camera")
        if camera.get("model") not in CAM_MODELS:
            problems.append("camera.model")
        if not isinstance(camera.get("name"), str) or not camera.get("name"):
            problems.append("camera.name")
        if not isinstance(camera.get("identifier"), str):
            problems.append("identifier")
        for key in ("transmitter", "alive"):
            if not isinstance(camera.get(key), bool):
                problems.append(key)
        fails = camera.get("fail_count")
        if isinstance(fails, bool) or not isinstance(fails, int) or fails < 0:
            problems.append("camera.fail_count")
        caps = camera.get("capabilities")
        if not isinstance(caps, dict) or set(caps) != set(CAM_CAP_KEYS):
            problems.append("camera.capabilities")
        elif any(not isinstance(caps.get(key), bool) for key in CAM_CAP_KEYS):
            problems.append("camera.capabilities тип")
    motion = need_obj("motion")
    if motion is not None:
        if set(motion) != {"rate", "orientation"}:
            problems.append("состав motion")
        rate = motion.get("rate")
        orient = motion.get("orientation")
        if isinstance(rate, bool) or not isinstance(rate, int) or not 1 <= rate <= 100:
            problems.append("rate")
        if not isinstance(orient, dict) or set(orient) != {"pan", "tilt", "zoom", "focus"}:
            problems.append("orientation")
        elif any(not isinstance(orient.get(key), bool) for key in ("pan", "tilt", "zoom", "focus")):
            problems.append("orientation тип")
    presets = need_obj("presets")
    if presets is not None:
        if set(presets) != {"latest", "active"}:
            problems.append("состав presets")
        latest = presets.get("latest")
        if latest is not None and (isinstance(latest, bool) or not isinstance(latest, int) or not 1 <= latest <= 99):
            problems.append("presets.latest")
        if not isinstance(presets.get("active"), bool):
            problems.append("presets.active")
    limits = need_obj("limits")
    if limits is not None:
        names = {"tilt_up", "tilt_down", "pan_left", "pan_right"}
        if set(limits) != names or any(limits.get(key) not in {"unknown", "clear", "set"} for key in names):
            problems.append("limits")
    return problems


def run_status(client: Client, stats: Stats) -> None:
    anon = client.request("GET", "/api/status")
    expect_error(stats, "Status без доступа", anon, 401, "unauthorized", "unauthorized")
    if "no-store" not in anon.cache_control.lower():
        stats.fail("Status 401 Cache-Control", anon.cache_control or "пусто")
    else:
        stats.ok("Status 401 Cache-Control")

    modes = (
        ("none", False, None),
        ("cookie", False, None),
        ("bad-cookie-ui", False, None),
        ("bad-bearer", False, None),
        ("cookie-bad-bearer", False, None),
        ("cookie-ui", True, False),
        ("bearer", True, True),
        ("cookie-bearer", True, True),
        ("bad-cookie-bearer", True, True),
    )
    for mode, ok, service in modes:
        name = f"Status доступ: {mode}"
        headers = client.headers(mode)
        if headers is None:
            stats.skip(name, "нет credentials")
            continue
        if ok and service is False and not client.cookie:
            stats.skip(name, "нет operator cookie")
            continue
        if ok and service is True and not client.token:
            stats.skip(name, "нет Bearer")
            continue
        if not ok and "bearer" in mode and "bad-bearer" not in mode and not client.token and mode != "cookie-bad-bearer":
            stats.skip(name, "нет Bearer")
            continue
        reply = client.request("GET", "/api/status", headers=headers)
        if not ok:
            expect_error(stats, name, reply, 401, "unauthorized", "unauthorized")
            continue
        body = reply.json_body if isinstance(reply.json_body, dict) else None
        data = body.get("data") if isinstance(body, dict) else None
        found = status_problems(data, bool(service))
        if reply.status != 200 or body is None or body.get("ok") is not True or found:
            stats.fail(name, "; ".join(found) or f"HTTP {reply.status}")
        elif "no-store" not in reply.cache_control.lower():
            stats.fail(name, "нет Cache-Control no-store")
        else:
            stats.ok(name)

    headers = client.headers("cookie-ui")
    if headers is None:
        stats.skip("серия Status 100", "нет operator cookie")
        return
    client.request("GET", "/api/auth/token", headers=headers)
    previous = None
    for index in range(100):
        reply = client.request("GET", "/api/status", headers=headers)
        body = reply.json_body if isinstance(reply.json_body, dict) else None
        data = body.get("data") if isinstance(body, dict) else None
        found = status_problems(data, False)
        if reply.status != 200 or found:
            stats.fail(
                "серия Status 100",
                f"#{index + 1}: " + ("; ".join(found) or f"HTTP {reply.status}"),
            )
            return
        uptime = data.get("uptime_seconds") if isinstance(data, dict) else None
        if previous is not None and isinstance(uptime, int) and uptime + 1 < previous:
            stats.fail("серия Status 100", f"#{index + 1}: uptime уменьшился")
            return
        if isinstance(uptime, int):
            previous = uptime
        if index + 1 < 100:
            time.sleep(0.05)
    stats.ok("серия Status 100")


def run_idle(client: Client, stats: Stats, expire: bool) -> None:
    hdrs = client.headers("cookie-ui")
    if hdrs is None:
        for name in (
            "probe не продлевает idle",
            "неавторизованный запрос не продлевает idle",
            "заглушка 500 не продлевает idle",
            "пассивный status не продлевает idle",
            "чужой X-Esp-Active не продлевает idle",
            "X-Esp-Active 0 не продлевает idle",
            "ошибка status не продлевает idle",
            "seconds_left после продления",
            "Service с Active не продлевает idle",
            "пассивный status перед продлением",
            "X-Esp-Active продлевает idle",
            "иной X-Esp-Active не продлевает idle",
            "ошибка TTL не продлевает idle",
            "запрещённая операция не продлевает idle",
            "Service не продлевает idle",
            "GET token продлевает idle",
        ):
            stats.skip(name, "нет operator cookie")
        stats.skip("истечение session idle", "нет operator cookie")
        return

    def probe() -> int | None:
        reply = client.request("GET", "/api/auth/session", headers=hdrs)
        return seconds_left(reply)

    time.sleep(3)
    baseline = probe()
    if baseline is None:
        stats.fail("чтение seconds_left", "probe не показал operator")
        return

    def check(name: str, before: int) -> int | None:
        after = probe()
        if after is None:
            stats.fail(name, "probe потерял operator")
            return None
        if after > before + 1:
            stats.fail(name, f"seconds_left вырос с {before} до {after}")
            return after
        stats.ok(name)
        return after

    # Повторный probe сам не должен вернуть таймер к ttl.
    left = check("probe не продлевает idle", baseline)
    if left is None:
        return
    client.request("GET", "/api/aim")
    left = check("неавторизованный запрос не продлевает idle", left)
    if left is None:
        return
    client.request("GET", "/api/status")
    left = check("ошибка status не продлевает idle", left)
    if left is None:
        return
    client.request("GET", "/api/aim", headers=hdrs)
    left = check("заглушка 500 не продлевает idle", left)
    if left is None:
        return
    client.request("GET", "/api/status", headers=hdrs)
    left = check("пассивный status не продлевает idle", left)
    if left is None:
        return
    client.request("GET", "/api/status", headers=hdrs | {"X-Esp-Active": "2"})
    left = check("чужой X-Esp-Active не продлевает idle", left)
    if left is None:
        return
    client.request(
        "PUT",
        "/api/auth/session/ttl",
        headers=ui_headers({"Content-Type": "application/json", "Cookie": f"esp_sess={client.cookie}"}),
        body=b'{"ttl_seconds":29}',
    )
    left = check("ошибка TTL не продлевает idle", left)
    if left is None:
        return
    bearer = client.headers("bearer")
    if bearer is None:
        stats.skip("запрещённая операция не продлевает idle", "нет Bearer")
        stats.skip("Service не продлевает idle", "нет Bearer")
        stats.skip("Service с Active не продлевает idle", "нет Bearer")
    else:
        client.request("GET", "/api/auth/token", headers=bearer)
        left = check("запрещённая операция не продлевает idle", left)
        if left is None:
            return
        client.request("GET", "/api/aim", headers=bearer)
        left = check("Service не продлевает idle", left)
        if left is None:
            return

    def extended(name: str, before: int) -> int | None:
        after = probe()
        if after is None:
            stats.fail(name, "probe потерял operator")
            return None
        if after <= before:
            stats.fail(name, f"seconds_left не вырос: {before} -> {after}")
            return after
        stats.ok(name)
        return after

    client.request("GET", "/api/status", headers=hdrs)
    left = check("пассивный status перед продлением", left)
    if left is None:
        return
    client.request("GET", "/api/status", headers=hdrs | {"X-Esp-Active": "0"})
    left = check("X-Esp-Active 0 не продлевает idle", left)
    if left is None:
        return
    active = dict(hdrs)
    active["X-Esp-Active"] = "1"
    active_reply = client.request("GET", "/api/status", headers=active)
    left = extended("X-Esp-Active продлевает idle", left)
    if left is None:
        return
    shown_body = active_reply.json_body if isinstance(active_reply.json_body, dict) else None
    shown_data = shown_body.get("data") if isinstance(shown_body, dict) else None
    shown_session = shown_data.get("session") if isinstance(shown_data, dict) else None
    shown = shown_session.get("seconds_left") if isinstance(shown_session, dict) else None
    shown_ttl = shown_session.get("ttl_seconds") if isinstance(shown_session, dict) else None
    if (
        not isinstance(shown, int)
        or not isinstance(shown_ttl, int)
        or shown > shown_ttl
        or abs(shown - left) > 1
    ):
        stats.fail("seconds_left после продления", f"ответ {shown} ttl {shown_ttl} probe {left}")
    else:
        stats.ok("seconds_left после продления")
    other = dict(hdrs)
    other["X-Esp-Active"] = "2"
    client.request("GET", "/api/status", headers=other)
    left = check("иной X-Esp-Active не продлевает idle", left)
    if left is None:
        return
    if bearer is not None:
        service_status = dict(bearer)
        service_status["X-Esp-Active"] = "1"
        if client.cookie:
            service_status["Cookie"] = f"esp_sess={client.cookie}"
            service_status["X-Esp-Client"] = UI
        client.request("GET", "/api/status", headers=service_status)
        left = check("Service с Active не продлевает idle", left)
        if left is None:
            return
    time.sleep(1.2)
    settled = probe()
    if settled is None:
        stats.fail("GET token продлевает idle", "probe потерял operator")
        return
    left = settled
    client.request("GET", "/api/auth/token", headers=hdrs)
    left = extended("GET token продлевает idle", left)
    if left is None:
        return

    expire_name = "истечение session idle"
    if not expire:
        stats.skip(expire_name, "долгий тест, нужен --expire-session")
        return
    current = probe()
    if current is None:
        stats.fail(expire_name, "нет seconds_left")
        return
    deadline = time.monotonic() + current + 20
    while time.monotonic() < deadline:
        reply = client.request("GET", "/api/auth/session", headers=hdrs)
        body = reply.json_body if isinstance(reply.json_body, dict) else {}
        data = body.get("data") if isinstance(body, dict) else None
        if isinstance(data, dict) and data.get("method") == "none":
            stats.ok(expire_name)
            return
        time.sleep(5)
    stats.fail(expire_name, "сессия не истекла в пределах seconds_left + 20 с")


def run_framing(client: Client, stats: Stats) -> None:
    def case(name: str, fn) -> None:
        try:
            detail = fn()
        except Exception as exc:
            stats.fail(name, str(exc))
            return
        if detail:
            stats.fail(name, detail)
        else:
            stats.ok(name)

    def read_response(sock: socket.socket) -> tuple[int, dict | None, str]:
        data = b""
        while b"\r\n\r\n" not in data:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if len(data) > 65536:
                break
        head, _, rest = data.partition(b"\r\n\r\n")
        length = 0
        for line in head.split(b"\r\n")[1:]:
            if line.lower().startswith(b"content-length:"):
                length = int(line.split(b":", 1)[1].strip() or b"0")
        while len(rest) < length:
            chunk = sock.recv(max(1, length - len(rest)))
            if not chunk:
                break
            rest += chunk
        line = head.split(b"\r\n", 1)[0].decode("latin1", "replace") if head else ""
        parts = line.split()
        code = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 0
        try:
            parsed = json.loads(rest.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            parsed = None
        text = rest[:180].decode("latin1", "replace")
        return code, parsed if isinstance(parsed, dict) else None, text

    def connection_closed(sock: socket.socket) -> bool:
        follow = (
            f"GET /api/after-rejected-frame HTTP/1.1\r\nHost: {client.host}\r\n"
            "Connection: close\r\n\r\n"
        ).encode()
        try:
            sock.sendall(follow)
            return sock.recv(1024) == b""
        except OSError:
            return True

    def reject(payload: bytes, timeout: float) -> tuple[int, dict | None, str, bool]:
        sock = socket.create_connection((client.host, client.port), timeout)
        sock.settimeout(timeout)
        try:
            sock.sendall(payload)
            status, body, text = read_response(sock)
            return status, body, text, connection_closed(sock)
        finally:
            sock.close()

    def json_error(body: dict | None) -> tuple[str | None, str | None]:
        err = body.get("error") if isinstance(body, dict) else None
        if not isinstance(err, dict):
            return None, None
        code = err.get("code")
        message = err.get("message")
        return code if isinstance(code, str) else None, message if isinstance(message, str) else None

    def post(extra: str, body: bytes = b"") -> bytes:
        return (
            f"POST /api/motion HTTP/1.1\r\nHost: {client.host}\r\n"
            f"{extra}\r\nConnection: keep-alive\r\n\r\n"
        ).encode() + body

    def oversized_with_byte() -> str:
        status, body, text, closed = reject(
            post("Content-Type: application/json\r\nContent-Length: 2048", b"{"),
            client.timeout,
        )
        code, message = json_error(body)
        if status != 400 or code != "invalid_parameter" or message != "body too large":
            return f"HTTP {status} code={code!r} message={message!r} {text}"
        if not closed:
            return "соединение не закрыто"
        return ""

    def oversized_without_body() -> str:
        status, _body, text, _closed = reject(
            post("Content-Type: application/json\r\nContent-Length: 2048"),
            EMPTY_BODY_TIMEOUT_S,
        )
        if status != 408:
            return f"HTTP {status} вместо 408 {text}"
        return ""

    def chunked() -> str:
        status, body, text, closed = reject(
            post("Transfer-Encoding: chunked", b"0\r\n\r\n"),
            client.timeout,
        )
        code, message = json_error(body)
        if status != 400 or code != "invalid_parameter" or message != "unsupported transfer encoding":
            return f"HTTP {status} code={code!r} message={message!r} {text}"
        if not closed:
            return "соединение не закрыто"
        return ""

    def keep_alive() -> str:
        conn = http.client.HTTPConnection(client.host, client.port, timeout=client.timeout)
        try:
            conn.request("GET", "/api/keep-alive-one")
            first = conn.getresponse()
            first.read()
            conn.request("GET", "/api/keep-alive-two")
            second = conn.getresponse()
            second.read()
        finally:
            conn.close()
        if first.status == 404 and second.status == 404:
            return ""
        return f"{first.status} затем {second.status}"

    case("Content-Length 2048 и байт тела", oversized_with_byte)
    case("Content-Length 2048 без тела", oversized_without_body)
    case("Transfer-Encoding chunked", chunked)
    reply = client.request("GET", "/api/after-close-recovery")
    expect_error(stats, "сервер доступен на новом соединении", reply, 404, "not_found", "not found")
    case("keep-alive для обычных запросов", keep_alive)


def run_legacy(client: Client, stats: Stats) -> None:
    page = client.legacy_get("/")
    if page.status == 200 and "text/html" in page.content_type.lower() and b"<html" in page.body.lower():
        stats.ok("старая страница UI")
    else:
        stats.fail("старая страница UI", snippet(page))

    status = client.legacy_get("/api/status")
    expect_error(stats, "GET /api/status без доступа", status, 401, "unauthorized", "unauthorized")
    legacy_paths = [
        ("/api/ptz", "старый /api/ptz без cmd"),
        ("/api/cam", "старый /api/cam без cmd"),
        ("/api/auth", "старый /api/auth без cmd"),
        ("/api/net", "старый /api/net без cmd"),
    ]
    for path, name in legacy_paths:
        reply = client.legacy_get(path)
        body = reply.json_body if isinstance(reply.json_body, dict) else {}
        if (
            reply.status == 401
            and body.get("status") == "auth"
            and "error" not in body
        ):
            stats.ok(name)
        else:
            stats.fail(name, snippet(reply))


def ui_headers(extra: dict[str, str] | None = None) -> dict[str, str]:
    headers = {"X-Esp-Client": UI}
    if extra:
        headers.update(extra)
    return headers


def login_body(client: Client, payload: bytes, headers: dict[str, str] | None = None) -> Reply:
    return client.request(
        "POST",
        "/api/auth/login",
        headers=headers if headers is not None else ui_headers({"Content-Type": "application/json"}),
        body=payload,
    )


def session_cookie(header: str) -> str:
    parts = [part.strip() for part in header.split(";") if part.strip()]
    if not parts or not parts[0].startswith("esp_sess="):
        return ""
    token = parts[0].split("=", 1)[1]
    if re.fullmatch(r"[A-Za-z0-9]{32}", token) is None:
        return ""
    rest = {part.lower() for part in parts[1:]}
    if "path=/" not in rest or "httponly" not in rest or "samesite=lax" not in rest:
        return ""
    return token


def make_proof(salt_hex: str, nonce_hex: str, password: str) -> str:
    stored = hashlib.sha256(bytes.fromhex(salt_hex) + password.encode("utf-8")).digest()
    return hashlib.sha256(bytes.fromhex(nonce_hex) + stored).hexdigest()


def run_login_body(client: Client, stats: Stats) -> None:
    expect_error(
        stats,
        "challenge без X-Esp-Client",
        client.request("POST", "/api/auth/challenge"),
        401,
        "unauthorized",
        "unauthorized",
    )
    expect_error(
        stats,
        "login без X-Esp-Client",
        client.request(
            "POST",
            "/api/auth/login",
            headers={"Content-Type": "application/json"},
            body=b'{"user":"admin","proof":"%s"}' % PROOF_FILL.encode(),
        ),
        401,
        "unauthorized",
        "unauthorized",
    )

    cases: list[tuple[str, bytes, str]] = [
        ("JSON пустой объект", b"{}", "invalid parameter"),
        ("JSON null", b"null", "json object required"),
        ("null в поле", f'{{"user":null,"proof":"{PROOF_FILL}"}}'.encode(), "invalid parameter"),
        ("неверный тип", f'{{"user":1,"proof":"{PROOF_FILL}"}}'.encode(), "invalid parameter"),
        ("пустой user", f'{{"user":"","proof":"{PROOF_FILL}"}}'.encode(), "invalid parameter"),
        ("user длиннее 32", f'{{"user":"{"a" * 33}","proof":"{PROOF_FILL}"}}'.encode(), "invalid parameter"),
        ("proof не hex", b'{"user":"admin","proof":"zz"}', "invalid parameter"),
        ("неизвестное поле", f'{{"user":"admin","proof":"{PROOF_FILL}","extra":1}}'.encode(), "invalid parameter"),
        (
            "повторяющиеся ключи",
            f'{{"user":"admin","user":"admin","proof":"{PROOF_FILL}"}}'.encode(),
            "duplicate key",
        ),
        ("данные после объекта", f'{{"user":"admin","proof":"{PROOF_FILL}"}}[]'.encode(), "invalid json"),
        ("встроенный NUL", b'{"user":"a\x00b","proof":"' + PROOF_FILL.encode() + b'"}', "invalid json"),
        (
            "экранированный \\u0000",
            f'{{"user":"a\\u0000b","proof":"{PROOF_FILL}"}}'.encode(),
            "invalid json",
        ),
    ]
    for name, payload, message in cases:
        reply = login_body(client, payload)
        if reply.set_cookie:
            stats.fail(name, "Set-Cookie при ошибке формата | " + snippet(reply))
            continue
        expect_error(stats, name, reply, 400, "invalid_parameter", message)
    empty = client.request(
        "POST",
        "/api/auth/login",
        headers=ui_headers({"Content-Type": "application/json", "Content-Length": "0"}),
        body=b"",
    )
    expect_error(
        stats,
        "Content-Length 0 при чтении объекта",
        empty,
        400,
        "invalid_parameter",
        "body required",
    )

    core = f'{{"user":"admin","proof":"{PROOF_FILL}","extra":1}}'.encode()
    wide = core + b" " * (1024 - len(core))
    reply = login_body(client, wide)
    expect_error(stats, "Content-Length 1024", reply, 400, "invalid_parameter", "invalid parameter")

    slow_name = "таймаут медленного тела"
    try:
        # Первый байт нужен, чтобы ESP-IDF вызвал обработчик. Без него
        # разборщик сам отвечает 408 и http_read_object не начинается.
        header = (
            f"POST /api/auth/login HTTP/1.1\r\nHost: {client.host}\r\n"
            "Content-Type: application/json\r\n"
            f"Content-Length: {len(core)}\r\n"
            "X-Esp-Client: ui\r\nConnection: close\r\n\r\n"
        ).encode()
        sock = socket.create_connection((client.host, client.port), 12)
        sock.settimeout(12)
        try:
            sock.sendall(header + core[:1])
            data = b""
            while b"\r\n\r\n" not in data and len(data) < 4096:
                chunk = sock.recv(1024)
                if not chunk:
                    break
                data += chunk
            head, _, rest = data.partition(b"\r\n\r\n")
            length = 0
            for line in head.split(b"\r\n")[1:]:
                if line.lower().startswith(b"content-length:"):
                    length = int(line.split(b":", 1)[1].strip() or b"0")
            while len(rest) < length:
                chunk = sock.recv(max(1, length - len(rest)))
                if not chunk:
                    break
                rest += chunk
            data = head + b"\r\n\r\n" + rest
        finally:
            sock.close()
        head, _, rest = data.partition(b"\r\n\r\n")
        line = head.split(b"\r\n", 1)[0].decode("latin1", "replace")
        code = int(line.split()[1]) if len(line.split()) > 1 and line.split()[1].isdigit() else 0
        try:
            parsed = json.loads(rest.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            parsed = None
        err = parsed.get("error") if isinstance(parsed, dict) else None
        if (
            code == 400
            and isinstance(err, dict)
            and err.get("code") == "invalid_parameter"
            and err.get("message") == "body timeout"
        ):
            stats.ok(slow_name)
        else:
            stats.fail(slow_name, f"HTTP {code} {rest[:120]!r}")
    except (OSError, ValueError) as exc:
        stats.fail(slow_name, str(exc))


def run_auth_flow(client: Client, stats: Stats) -> None:
    flow_names = [
        "новый challenge",
        "legacy challenge",
        "некорректный proof гасит nonce",
        "неизвестный пользователь гасит nonce",
        "user из 31 символа гасит nonce",
        "user из 32 символов гасит nonce",
        "новый challenge гасит legacy nonce",
        "legacy challenge гасит новый nonce",
        "некорректный JSON сохраняет nonce",
        "новый challenge → новый login",
        "SessionView после нового login",
        "legacy принимает cookie нового login",
        "legacy challenge → legacy login",
        "probe принимает cookie legacy login",
        "legacy challenge → новый login",
        "новый challenge → legacy login",
    ]
    if not allow_login():
        for name in flow_names:
            stats.skip(name, ALLOW_REASON)
        return
    print(
        "ESP_CAM_ALLOW_LOGIN=1: challenge сменяет nonce, успешный login может остановить оси",
        file=sys.stderr,
    )
    password = os.environ.get("ESP_CAM_PASSWORD", "")

    new_ch = client.request("POST", "/api/auth/challenge", headers=ui_headers())
    expect_challenge(stats, "новый challenge", new_ch)
    old_ch = client.request("GET", "/api/auth?cmd=challenge", headers=ui_headers())
    old_body = old_ch.json_body if isinstance(old_ch.json_body, dict) else {}
    if (
        old_ch.status == 200
        and old_body.get("ok") is True
        and HEX32.fullmatch(str(old_body.get("salt", "")))
        and HEX32.fullmatch(str(old_body.get("nonce", "")))
        and HEX32.fullmatch(str(old_body.get("newsalt", "")))
        and "error" not in old_body
    ):
        stats.ok("legacy challenge")
    else:
        stats.fail("legacy challenge", snippet(old_ch))

    proof_names = flow_names[2:]
    if not password:
        for name in proof_names:
            stats.skip(name, "нет ESP_CAM_PASSWORD")
        return

    def rejected(name: str, reply: Reply) -> None:
        if reply.set_cookie:
            stats.fail(name, "Set-Cookie при отказе | " + snippet(reply))
            return
        expect_error(stats, name, reply, 401, "unauthorized", "unauthorized")

    def legacy_rejected(name: str, reply: Reply) -> None:
        body = reply.json_body if isinstance(reply.json_body, dict) else None
        if (
            not reply.error
            and reply.status == 401
            and isinstance(body, dict)
            and body.get("ok") is False
            and body.get("status") == "auth"
            and "error" not in body
            and not reply.set_cookie
        ):
            stats.ok(name)
            return
        stats.fail(name, snippet(reply))

    def fields_of(reply: Reply, root: bool, salt_key: str, nonce_key: str) -> tuple[str, str] | None:
        body = reply.json_body if isinstance(reply.json_body, dict) else None
        if not isinstance(body, dict) or body.get("ok") is not True:
            return None
        data = body if root else body.get("data")
        if not isinstance(data, dict):
            return None
        salt = data.get(salt_key)
        nonce = data.get(nonce_key)
        if not isinstance(salt, str) or not isinstance(nonce, str):
            return None
        return salt, nonce

    fresh = client.request("POST", "/api/auth/challenge", headers=ui_headers())
    pair = fields_of(fresh, False, "salt", "nonce")
    if pair is None:
        stats.fail("некорректный proof гасит nonce", snippet(fresh))
    else:
        salt, nonce = pair
        bad = login_body(
            client,
            json.dumps({"user": client.user, "proof": PROOF_FILL}).encode(),
        )
        rejected("некорректный proof гасит nonce", bad)
        again = login_body(
            client,
            json.dumps({"user": client.user, "proof": make_proof(salt, nonce, password)}).encode(),
        )
        rejected("верный proof после неверного", again)

    fresh = client.request("POST", "/api/auth/challenge", headers=ui_headers())
    pair = fields_of(fresh, False, "salt", "nonce")
    if pair is None:
        stats.fail("неизвестный пользователь гасит nonce", snippet(fresh))
    else:
        salt, nonce = pair
        wrong_user = login_body(
            client,
            json.dumps({"user": "no-such-user", "proof": make_proof(salt, nonce, password)}).encode(),
        )
        rejected("неизвестный пользователь гасит nonce", wrong_user)
        later = login_body(
            client,
            json.dumps({"user": client.user, "proof": make_proof(salt, nonce, password)}).encode(),
        )
        rejected("верный пользователь после чужого имени", later)

    fresh = client.request("POST", "/api/auth/challenge", headers=ui_headers())
    pair = fields_of(fresh, False, "salt", "nonce")
    if pair is None:
        stats.fail("user из 32 символов гасит nonce", snippet(fresh))
    else:
        salt, nonce = pair
        proof = make_proof(salt, nonce, password)
        rejected(
            "user из 32 символов гасит nonce",
            login_body(client, json.dumps({"user": "a" * 32, "proof": proof}).encode()),
        )
        rejected(
            "верный пользователь после user из 32",
            login_body(client, json.dumps({"user": client.user, "proof": proof}).encode()),
        )

    fresh = client.request("POST", "/api/auth/challenge", headers=ui_headers())
    pair = fields_of(fresh, False, "salt", "nonce")
    if pair is None:
        stats.fail("user из 31 символа гасит nonce", snippet(fresh))
    else:
        salt, nonce = pair
        proof = make_proof(salt, nonce, password)
        rejected(
            "user из 31 символа гасит nonce",
            login_body(client, json.dumps({"user": "b" * 31, "proof": proof}).encode()),
        )
        rejected(
            "верный пользователь после user из 31",
            login_body(client, json.dumps({"user": client.user, "proof": proof}).encode()),
        )

    legacy = client.request("GET", "/api/auth?cmd=challenge", headers=ui_headers())
    old_pair = fields_of(legacy, True, "salt", "nonce")
    client.request("POST", "/api/auth/challenge", headers=ui_headers())
    if old_pair is None:
        stats.fail("новый challenge гасит legacy nonce", snippet(legacy))
    else:
        salt, nonce = old_pair
        stale = login_body(
            client,
            json.dumps({"user": client.user, "proof": make_proof(salt, nonce, password)}).encode(),
        )
        rejected("новый challenge гасит legacy nonce", stale)

    fresh = client.request("POST", "/api/auth/challenge", headers=ui_headers())
    pair = fields_of(fresh, False, "salt", "nonce")
    client.request("GET", "/api/auth?cmd=challenge", headers=ui_headers())
    if pair is None:
        stats.fail("legacy challenge гасит новый nonce", snippet(fresh))
    else:
        salt, nonce = pair
        query = urllib.parse.urlencode(
            {"cmd": "login", "user": client.user, "proof": make_proof(salt, nonce, password)}
        )
        stale = client.request("GET", f"/api/auth?{query}", headers=ui_headers())
        legacy_rejected("legacy challenge гасит новый nonce", stale)

    def opened(name: str, reply: Reply) -> str:
        token = session_cookie(reply.set_cookie)
        body = reply.json_body if isinstance(reply.json_body, dict) else None
        data = body.get("data") if isinstance(body, dict) else None
        problems: list[str] = []
        if reply.status != 200 or not isinstance(body, dict) or body.get("ok") is not True:
            problems.append(snippet(reply))
        elif not isinstance(data, dict) or data.get("user") != client.user:
            problems.append(f"user {data.get('user') if isinstance(data, dict) else None!r}")
        elif not isinstance(data.get("ttl_seconds"), int) or not 30 <= data["ttl_seconds"] <= 300:
            problems.append(f"ttl {data.get('ttl_seconds')!r}")
        if not token:
            problems.append("нет Set-Cookie esp_sess")
        if problems:
            stats.fail(name, "; ".join(problems))
            return ""
        stats.ok(name)
        client.cookie = token
        return token

    def legacy_opened(name: str, reply: Reply) -> str:
        token = session_cookie(reply.set_cookie)
        body = reply.json_body if isinstance(reply.json_body, dict) else None
        if reply.status == 200 and isinstance(body, dict) and body.get("ok") is True and token:
            stats.ok(name)
            client.cookie = token
            return token
        stats.fail(name, snippet(reply))
        return ""

    fresh = client.request("POST", "/api/auth/challenge", headers=ui_headers())
    pair = fields_of(fresh, False, "salt", "nonce")
    if pair is None:
        stats.fail("некорректный JSON сохраняет nonce", snippet(fresh))
        stats.fail("новый challenge → новый login", "нет challenge")
        stats.skip("SessionView после нового login", "login не выполнен")
        stats.skip("legacy принимает cookie нового login", "login не выполнен")
    else:
        salt, nonce = pair
        broken = login_body(client, b'{"user":')
        broken_ok = (
            not broken.error
            and broken.status == 400
            and not broken.set_cookie
            and isinstance(broken.json_body, dict)
            and isinstance(broken.json_body.get("error"), dict)
            and broken.json_body["error"].get("code") == "invalid_parameter"
            and broken.json_body["error"].get("message") == "invalid json"
        )
        token = ""
        if broken_ok:
            token = opened(
                "новый challenge → новый login",
                login_body(
                    client,
                    json.dumps(
                        {"user": client.user, "proof": make_proof(salt, nonce, password)}
                    ).encode(),
                ),
            )
        else:
            stats.fail("новый challenge → новый login", "битый JSON не отклонён, login не вызывался")
        if broken_ok and token:
            stats.ok("некорректный JSON сохраняет nonce")
        elif not broken_ok:
            stats.fail("некорректный JSON сохраняет nonce", snippet(broken))
        else:
            stats.fail("некорректный JSON сохраняет nonce", "верный proof после битого JSON не принят")
        if not token:
            stats.skip("SessionView после нового login", "login не выполнен")
            stats.skip("legacy принимает cookie нового login", "login не выполнен")
        else:
            view = client.request(
                "GET",
                "/api/auth/session",
                headers={"X-Esp-Client": UI, "Cookie": f"esp_sess={token}"},
            )
            body = view.json_body if isinstance(view.json_body, dict) else None
            data = body.get("data") if isinstance(body, dict) else None
            problems = []
            if view.status != 200 or not isinstance(data, dict):
                problems.append(snippet(view))
            else:
                if data.get("authenticated") is not True or data.get("method") != "operator":
                    problems.append(f"method {data.get('method')!r}")
                if data.get("user") != client.user:
                    problems.append("user")
                if not isinstance(data.get("ttl_seconds"), int) or not 30 <= data["ttl_seconds"] <= 300:
                    problems.append("ttl")
                if not isinstance(data.get("seconds_left"), int) or data["seconds_left"] < 0:
                    problems.append("seconds_left")
                if data.get("ttl_min_seconds") != 30 or data.get("ttl_max_seconds") != 300:
                    problems.append("границы ttl")
            if problems:
                stats.fail("SessionView после нового login", "; ".join(problems))
            else:
                stats.ok("SessionView после нового login")
            me = client.legacy_get(
                "/api/auth?cmd=me",
                {"X-Esp-Client": UI, "Cookie": f"esp_sess={token}"},
            )
            me_body = me.json_body if isinstance(me.json_body, dict) else {}
            if me.status == 200 and me_body.get("ok") is True and me_body.get("session") is True:
                stats.ok("legacy принимает cookie нового login")
            else:
                stats.fail("legacy принимает cookie нового login", snippet(me))

    legacy = client.request("GET", "/api/auth?cmd=challenge", headers=ui_headers())
    old_pair = fields_of(legacy, True, "salt", "nonce")
    if old_pair is None:
        stats.fail("legacy challenge → legacy login", snippet(legacy))
        stats.skip("probe принимает cookie legacy login", "login не выполнен")
    else:
        salt, nonce = old_pair
        query = urllib.parse.urlencode(
            {"cmd": "login", "user": client.user, "proof": make_proof(salt, nonce, password)}
        )
        token = legacy_opened(
            "legacy challenge → legacy login",
            client.request("GET", f"/api/auth?{query}", headers=ui_headers()),
        )
        if not token:
            stats.skip("probe принимает cookie legacy login", "login не выполнен")
        else:
            view = client.request(
                "GET",
                "/api/auth/session",
                headers={"X-Esp-Client": UI, "Cookie": f"esp_sess={token}"},
            )
            expect_probe(stats, "probe принимает cookie legacy login", view, "operator")

    legacy = client.request("GET", "/api/auth?cmd=challenge", headers=ui_headers())
    old_pair = fields_of(legacy, True, "salt", "nonce")
    if old_pair is None:
        stats.fail("legacy challenge → новый login", snippet(legacy))
    else:
        salt, nonce = old_pair
        opened(
            "legacy challenge → новый login",
            login_body(
                client,
                json.dumps({"user": client.user, "proof": make_proof(salt, nonce, password)}).encode(),
            ),
        )

    fresh = client.request("POST", "/api/auth/challenge", headers=ui_headers())
    pair = fields_of(fresh, False, "salt", "nonce")
    if pair is None:
        stats.fail("новый challenge → legacy login", snippet(fresh))
    else:
        salt, nonce = pair
        query = urllib.parse.urlencode(
            {"cmd": "login", "user": client.user, "proof": make_proof(salt, nonce, password)}
        )
        legacy_opened(
            "новый challenge → legacy login",
            client.request("GET", f"/api/auth?{query}", headers=ui_headers()),
        )

    # Истекший nonce ждёт 61 с. Это делается после проверки idle: при TTL 60 с
    # пауза здесь погасила бы operator session до измерения seconds_left.


def run_expire_nonce(client: Client, stats: Stats, enabled: bool) -> None:
    expire_name = "истёкший nonce"
    if not allow_login():
        stats.skip(expire_name, ALLOW_REASON)
        return
    if not enabled:
        stats.skip(expire_name, "долгий тест, нужен --expire-nonce")
        return
    password = os.environ.get("ESP_CAM_PASSWORD", "")
    if not password:
        stats.skip(expire_name, "нет ESP_CAM_PASSWORD")
        return
    fresh = client.request("POST", "/api/auth/challenge", headers=ui_headers())
    body = fresh.json_body if isinstance(fresh.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    salt = data.get("salt") if isinstance(data, dict) else None
    nonce = data.get("nonce") if isinstance(data, dict) else None
    if (
        fresh.status != 200
        or not isinstance(salt, str)
        or not isinstance(nonce, str)
        or HEX32.fullmatch(salt) is None
        or HEX32.fullmatch(nonce) is None
    ):
        stats.fail(expire_name, snippet(fresh))
        return
    time.sleep(61)
    late = login_body(
        client,
        json.dumps({"user": client.user, "proof": make_proof(salt, nonce, password)}).encode(),
    )
    if late.set_cookie:
        stats.fail(expire_name, "Set-Cookie при отказе | " + snippet(late))
        return
    expect_error(stats, expire_name, late, 401, "unauthorized", "unauthorized")


def challenge_fields(client: Client) -> tuple[str, str, str] | None:
    reply = client.request("POST", "/api/auth/challenge", headers=ui_headers())
    body = reply.json_body if isinstance(reply.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    if reply.status != 200 or not isinstance(data, dict):
        return None
    salt = data.get("salt")
    nonce = data.get("nonce")
    nxt = data.get("next_salt")
    if not all(isinstance(item, str) and HEX32.fullmatch(item) for item in (salt, nonce, nxt)):
        return None
    assert isinstance(salt, str) and isinstance(nonce, str) and isinstance(nxt, str)
    return salt, nonce, nxt


def password_hash(next_salt_hex: str, password: str) -> str:
    return hashlib.sha256(bytes.fromhex(next_salt_hex) + password.encode("utf-8")).hexdigest()


def open_operator(client: Client, password: str) -> bool:
    fields = challenge_fields(client)
    if fields is None:
        return False
    salt, nonce, _nxt = fields
    reply = login_body(
        client,
        json.dumps({"user": client.user, "proof": make_proof(salt, nonce, password)}).encode(),
    )
    token = session_cookie(reply.set_cookie)
    body = reply.json_body if isinstance(reply.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    if reply.status != 200 or not token or not isinstance(data, dict) or data.get("user") != client.user:
        return False
    client.cookie = token
    return True


def operator_headers(client: Client) -> dict[str, str] | None:
    headers = client.headers("cookie-ui")
    if headers is None:
        return None
    headers["Content-Type"] = "application/json"
    return headers


def cookie_cleared(header: str) -> bool:
    parts = [part.strip() for part in header.split(";") if part.strip()]
    if not parts or not parts[0].lower().startswith("esp_sess="):
        return False
    value = parts[0].split("=", 1)[1]
    if value not in ("", '""'):
        return False
    rest = {part.lower() for part in parts[1:]}
    return "path=/" in rest and "max-age=0" in rest and "httponly" in rest and "samesite=lax" in rest


def run_auth_forms(client: Client, stats: Stats) -> None:
    expect_error(
        stats,
        "password без доступа",
        client.request("PUT", "/api/auth/password", body=b"{}"),
        401,
        "unauthorized",
        "unauthorized",
    )
    expect_error(
        stats,
        "TTL без доступа",
        client.request("PUT", "/api/auth/session/ttl", body=b"{}"),
        401,
        "unauthorized",
        "unauthorized",
    )
    bearer = client.headers("bearer")
    if bearer is None:
        stats.skip("password с Bearer", "нет Bearer")
        stats.skip("TTL с Bearer", "нет Bearer")
    else:
        expect_error(
            stats,
            "password с Bearer",
            client.request("PUT", "/api/auth/password", headers=bearer, body=b"{}"),
            403,
            "forbidden",
            "forbidden",
        )
        expect_error(
            stats,
            "TTL с Bearer",
            client.request("PUT", "/api/auth/session/ttl", headers=bearer, body=b"{}"),
            403,
            "forbidden",
            "forbidden",
        )
    headers = operator_headers(client)
    if headers is None:
        for name in (
            "password пустой объект",
            "password неверный proof",
            "TTL ниже диапазона",
            "TTL выше диапазона",
            "TTL дробный",
            "TTL строка",
            "некорректный JSON пароля сохраняет nonce",
        ):
            stats.skip(name, "нет operator cookie")
        return
    expect_error(
        stats,
        "password пустой объект",
        client.request("PUT", "/api/auth/password", headers=headers, body=b"{}"),
        400,
        "invalid_parameter",
        "invalid parameter",
    )
    expect_error(
        stats,
        "password неверный proof",
        client.request(
            "PUT",
            "/api/auth/password",
            headers=headers,
            body=json.dumps({"proof": "zz", "new_password_hash": PROOF_FILL}).encode(),
        ),
        400,
        "invalid_parameter",
        "invalid parameter",
    )
    for name, payload in (
        ("TTL ниже диапазона", {"ttl_seconds": 29}),
        ("TTL выше диапазона", {"ttl_seconds": 301}),
        ("TTL дробный", {"ttl_seconds": 30.5}),
        ("TTL строка", {"ttl_seconds": "60"}),
    ):
        expect_error(
            stats,
            name,
            client.request(
                "PUT",
                "/api/auth/session/ttl",
                headers=headers,
                body=json.dumps(payload).encode(),
            ),
            400,
            "invalid_parameter",
            "invalid parameter",
        )
    nonce_name = "некорректный JSON пароля сохраняет nonce"
    password = os.environ.get("ESP_CAM_PASSWORD", "")
    if not allow_login() or not password:
        stats.skip(nonce_name, ALLOW_REASON if not allow_login() else "нет ESP_CAM_PASSWORD")
        return
    fields = challenge_fields(client)
    if fields is None:
        stats.fail(nonce_name, "нет challenge")
        return
    salt, nonce, _nxt = fields
    broken = client.request("PUT", "/api/auth/password", headers=headers, body=b'{"proof":')
    if broken.status != 400:
        stats.fail(nonce_name, f"HTTP {broken.status}")
        return
    opened = login_body(
        client,
        json.dumps({"user": client.user, "proof": make_proof(salt, nonce, password)}).encode(),
    )
    cookie = session_cookie(opened.set_cookie)
    if opened.status == 200 and cookie:
        client.cookie = cookie
        stats.ok(nonce_name)
    else:
        stats.fail(nonce_name, f"HTTP {opened.status} login не принял nonce")


def run_auth_write(client: Client, stats: Stats) -> None:
    names = (
        "смена пароля и восстановление",
        "старый пароль не входит",
        "смена TTL и восстановление",
        "перевыпуск token гасит прежний",
        "прежний Bearer отклонён",
    )
    if not allow_auth_write():
        for name in names:
            stats.skip(name, WRITE_REASON)
        stats.skip("отказ записи NVS", "нет способа вызвать ошибку NVS без тестового endpoint")
        return
    password = os.environ.get("ESP_CAM_PASSWORD", "")
    headers = operator_headers(client)
    if headers is None or not password:
        for name in names:
            stats.skip(name, "нет operator cookie или пароля")
        return
    print(
        "ESP_CAM_ALLOW_AUTH_WRITE=1: пароль и TTL будут изменены и восстановлены; "
        "перевыпуск token необратим",
        file=sys.stderr,
    )

    temp = "t" + os.urandom(8).hex()
    fields = challenge_fields(client)
    changed = False
    if fields is None:
        stats.fail("смена пароля и восстановление", "нет challenge")
    else:
        salt, nonce, nxt = fields
        reply = client.request(
            "PUT",
            "/api/auth/password",
            headers=headers,
            body=json.dumps(
                {
                    "proof": make_proof(salt, nonce, password),
                    "new_password_hash": password_hash(nxt, temp),
                }
            ).encode(),
        )
        body = reply.json_body if isinstance(reply.json_body, dict) else None
        data = body.get("data") if isinstance(body, dict) else None
        changed = (
            not reply.error
            and reply.status == 200
            and isinstance(data, dict)
            and data.get("applied") is True
        )
        if not changed:
            stats.fail("смена пароля и восстановление", f"HTTP {reply.status}")
    if changed:
        stale = challenge_fields(client)
        if stale is None:
            stats.fail("старый пароль не входит", "нет challenge")
        else:
            salt, nonce, _nxt = stale
            rejected = login_body(
                client,
                json.dumps(
                    {"user": client.user, "proof": make_proof(salt, nonce, password)}
                ).encode(),
            )
            if rejected.status == 401 and not rejected.set_cookie:
                stats.ok("старый пароль не входит")
            else:
                stats.fail("старый пароль не входит", f"HTTP {rejected.status}")
        if not open_operator(client, temp):
            stats.fail("смена пароля и восстановление", "вход новым паролем не удался")
        else:
            headers = operator_headers(client)
            back = challenge_fields(client) if headers is not None else None
            restored = False
            if back is not None and headers is not None:
                salt, nonce, nxt = back
                reply = client.request(
                    "PUT",
                    "/api/auth/password",
                    headers=headers,
                    body=json.dumps(
                        {
                            "proof": make_proof(salt, nonce, temp),
                            "new_password_hash": password_hash(nxt, password),
                        }
                    ).encode(),
                )
                restored = (
                    not reply.error
                    and reply.status == 200
                    and isinstance(reply.json_body, dict)
                    and isinstance(reply.json_body.get("data"), dict)
                    and reply.json_body["data"].get("applied") is True
                )
            if not restored:
                stats.fail("смена пароля и восстановление", "пароль не восстановлен")
            elif not open_operator(client, password):
                stats.fail("смена пароля и восстановление", "вход прежним паролем не удался")
            else:
                stats.ok("смена пароля и восстановление")
    else:
        stats.skip("старый пароль не входит", "пароль не сменён")

    headers = operator_headers(client)
    probe = client.request("GET", "/api/auth/session", headers=client.headers("cookie-ui") or {})
    pdata = probe.json_body.get("data") if isinstance(probe.json_body, dict) else None
    current = pdata.get("ttl_seconds") if isinstance(pdata, dict) else None
    if headers is None or not isinstance(current, int):
        stats.skip("смена TTL и восстановление", "нет сессии")
    else:
        target = 30 if current != 30 else 300
        reply = client.request(
            "PUT",
            "/api/auth/session/ttl",
            headers=headers,
            body=json.dumps({"ttl_seconds": target}).encode(),
        )
        tdata = reply.json_body.get("data") if isinstance(reply.json_body, dict) else None
        applied = isinstance(tdata, dict) and tdata.get("ttl_seconds") == target
        back = client.request(
            "PUT",
            "/api/auth/session/ttl",
            headers=headers,
            body=json.dumps({"ttl_seconds": current}).encode(),
        )
        bdata = back.json_body.get("data") if isinstance(back.json_body, dict) else None
        restored = isinstance(bdata, dict) and bdata.get("ttl_seconds") == current
        if applied and restored:
            stats.ok("смена TTL и восстановление")
        elif not restored:
            stats.fail("смена TTL и восстановление", "TTL не восстановлен")
        else:
            stats.fail("смена TTL и восстановление", f"HTTP {reply.status}")

    previous = client.token
    headers = operator_headers(client)
    if headers is None or not previous:
        stats.skip("перевыпуск token гасит прежний", "нет cookie или Bearer")
    else:
        reply = client.request("POST", "/api/auth/token/reissue", headers=client.headers("cookie-ui"))
        body = reply.json_body if isinstance(reply.json_body, dict) else None
        data = body.get("data") if isinstance(body, dict) else None
        issued = data.get("token") if isinstance(data, dict) else None
        if (
            reply.status != 200
            or not isinstance(issued, str)
            or TOKEN_RE.fullmatch(issued) is None
            or issued == previous
        ):
            stats.fail("перевыпуск token гасит прежний", f"HTTP {reply.status} token не заменён")
            stats.skip("прежний Bearer отклонён", "новый token не выпущен")
        else:
            stats.ok("перевыпуск token гасит прежний")
            client.token = issued
            old = client.request("GET", "/api/aim", headers={"Authorization": f"Bearer {previous}"})
            new = client.request("GET", "/api/aim", headers={"Authorization": f"Bearer {issued}"})
            if old.status == 401 and new.status == 500:
                stats.ok("прежний Bearer отклонён")
            else:
                stats.fail(
                    "прежний Bearer отклонён",
                    f"старый HTTP {old.status}, новый HTTP {new.status}",
                )
    stats.skip("отказ записи NVS", "нет способа вызвать ошибку NVS без тестового endpoint")


def run_after_restart(client: Client, stats: Stats, enabled: bool) -> None:
    if enabled:
        return
    stats.skip(
        "сохранение после перезапуска",
        "одним процессом: --after-restart, без смены сети",
    )


def http_up(client: Client) -> bool:
    conn: http.client.HTTPConnection | None = None
    try:
        sock = socket.create_connection((client.host, client.port), timeout=1.0)
        sock.settimeout(1.0)
        conn = http.client.HTTPConnection(client.host, client.port, timeout=1.0)
        conn.sock = sock
        conn.request("GET", "/api/auth/session")
        resp = conn.getresponse()
        resp.read()
        return resp.status == 200
    except (OSError, http.client.HTTPException):
        return False
    finally:
        if conn is not None:
            conn.close()


def wait_http(client: Client, want_up: bool, timeout_s: float, hold_s: float) -> bool:
    deadline = time.monotonic() + timeout_s
    seen: float | None = None
    while time.monotonic() < deadline:
        if http_up(client) == want_up:
            if seen is None:
                seen = time.monotonic()
            if time.monotonic() - seen >= hold_s:
                return True
        else:
            seen = None
        time.sleep(0.3)
    return False


def read_ttl(client: Client) -> int | None:
    probe = client.request("GET", "/api/auth/session", headers=client.headers("cookie-ui"))
    body = probe.json_body if isinstance(probe.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    ttl = data.get("ttl_seconds") if isinstance(data, dict) else None
    if data is None or data.get("method") != "operator" or not isinstance(ttl, int):
        return None
    if not 30 <= ttl <= 300:
        return None
    return ttl


def read_service_token(client: Client) -> str | None:
    reply = client.request("GET", "/api/auth/token", headers=client.headers("cookie-ui"))
    body = reply.json_body if isinstance(reply.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    token = data.get("token") if isinstance(data, dict) else None
    if reply.status != 200 or not isinstance(token, str) or TOKEN_RE.fullmatch(token) is None:
        return None
    return token


def bearer_accepted(client: Client, token: str) -> int:
    reply = client.request("GET", "/api/aim", headers={"Authorization": f"Bearer {token}"})
    return reply.status


def phase(text: str) -> None:
    print(f"фаза: {text}", flush=True)


def snapshot_credentials(client: Client, stats: Stats, password: str) -> tuple[str, int] | None:
    if not open_operator(client, password):
        stats.fail("до обновления: login", "сессия не открыта")
        return None
    stats.ok("до обновления: login")
    ttl = read_ttl(client)
    if ttl is None:
        stats.fail("до обновления: TTL", "не прочитан")
        return None
    stats.ok(f"до обновления: TTL прочитан ({ttl})")
    token = read_service_token(client)
    if token is None:
        stats.fail("до обновления: Service token", "не прочитан")
        return None
    client.token = token
    stats.ok("до обновления: Service token прочитан")
    status = bearer_accepted(client, token)
    if status != 500:
        stats.fail("до обновления: Bearer", f"HTTP {status}")
        return None
    stats.ok("до обновления: Bearer принят")
    return token, ttl


def compare_saved(
    client: Client,
    stats: Stats,
    prefix: str,
    password: str,
    token: str,
    ttl: int,
) -> bool:
    status = bearer_accepted(client, token)
    if status != 500:
        stats.fail(f"{prefix}: прежний Bearer до чтения token", f"HTTP {status}")
        ok = False
    else:
        stats.ok(f"{prefix}: прежний Bearer до чтения token")
        ok = True
    if not open_operator(client, password):
        stats.fail(f"{prefix}: login прежним паролем", "сессия не открыта")
        return False
    stats.ok(f"{prefix}: login прежним паролем")
    got_ttl = read_ttl(client)
    got_token = read_service_token(client)
    if got_ttl != ttl:
        stats.fail(f"{prefix}: TTL", f"{got_ttl} вместо {ttl}")
        ok = False
    else:
        stats.ok(f"{prefix}: TTL совпал ({ttl})")
    if got_token != token:
        stats.fail(f"{prefix}: Service token", "не совпал" if got_token else "не прочитан")
        ok = False
    else:
        stats.ok(f"{prefix}: Service token совпал")
    return ok


def roundtrip_password(client: Client, stats: Stats, password: str, token: str, ttl: int) -> bool:
    if not allow_auth_write():
        stats.skip("новый blob: смена пароля", WRITE_REASON)
        stats.skip("новый blob: старый пароль отклонён", WRITE_REASON)
        stats.skip("новый blob: пароль восстановлен", WRITE_REASON)
        return True
    headers = operator_headers(client)
    if headers is None:
        stats.fail("новый blob: смена пароля", "нет operator cookie")
        return False
    temp = "t" + os.urandom(8).hex()
    fields = challenge_fields(client)
    if fields is None:
        stats.fail("новый blob: смена пароля", "нет challenge")
        return False
    salt, nonce, nxt = fields
    reply = client.request(
        "PUT",
        "/api/auth/password",
        headers=headers,
        body=json.dumps(
            {
                "proof": make_proof(salt, nonce, password),
                "new_password_hash": password_hash(nxt, temp),
            }
        ).encode(),
    )
    body = reply.json_body if isinstance(reply.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    if reply.error or reply.status != 200 or not isinstance(data, dict) or data.get("applied") is not True:
        stats.fail("новый blob: смена пароля", f"HTTP {reply.status}")
        return False
    stats.ok("новый blob: смена пароля")
    stale = challenge_fields(client)
    if stale is None:
        stats.fail("новый blob: старый пароль отклонён", "нет challenge")
    else:
        salt, nonce, _nxt = stale
        rejected = login_body(
            client,
            json.dumps({"user": client.user, "proof": make_proof(salt, nonce, password)}).encode(),
        )
        if rejected.status == 401 and not rejected.set_cookie:
            stats.ok("новый blob: старый пароль отклонён")
        else:
            stats.fail("новый blob: старый пароль отклонён", f"HTTP {rejected.status}")
    if not open_operator(client, temp):
        stats.fail("новый blob: пароль восстановлен", "вход новым паролем не удался")
    headers = operator_headers(client)
    back = challenge_fields(client) if headers is not None else None
    restored = False
    if back is not None and headers is not None:
        salt, nonce, nxt = back
        reply = client.request(
            "PUT",
            "/api/auth/password",
            headers=headers,
            body=json.dumps(
                {
                    "proof": make_proof(salt, nonce, temp),
                    "new_password_hash": password_hash(nxt, password),
                }
            ).encode(),
        )
        restored = (
            not reply.error
            and reply.status == 200
            and isinstance(reply.json_body, dict)
            and isinstance(reply.json_body.get("data"), dict)
            and reply.json_body["data"].get("applied") is True
        )
    if not restored or not open_operator(client, password):
        stats.fail("новый blob: пароль восстановлен", "исходный пароль не восстановлен")
        return False
    stats.ok("новый blob: пароль восстановлен")
    got_ttl = read_ttl(client)
    got_token = read_service_token(client)
    if got_ttl != ttl:
        stats.fail("новый blob: TTL не изменился", f"{got_ttl} вместо {ttl}")
    else:
        stats.ok(f"новый blob: TTL не изменился ({ttl})")
    if got_token != token:
        stats.fail("новый blob: token не изменился", "не совпал" if got_token else "не прочитан")
    else:
        stats.ok("новый blob: token не изменился")
    status = bearer_accepted(client, token)
    if status != 500:
        stats.fail("новый blob: прежний Bearer", f"HTTP {status}")
    else:
        stats.ok("новый blob: прежний Bearer принят")
    return True


def wait_user_power(client: Client, confirm_file: str) -> bool:
    phase("выключите питание устройства. Сеть не меняйте")
    if not wait_http(client, False, 600, 3):
        return False
    phase("питание пропало. Включите устройство и подтвердите: введите OK")
    confirmed = {"ok": False}

    def read_ok() -> None:
        while True:
            line = sys.stdin.readline()
            if line == "":
                return
            if line.strip().lower() in {"ok", "да", "yes"}:
                confirmed["ok"] = True
                return

    reader = threading.Thread(target=read_ok, daemon=True)
    reader.start()
    deadline = time.monotonic() + 600
    up = False
    while time.monotonic() < deadline:
        if confirm_file and os.path.isfile(confirm_file):
            confirmed["ok"] = True
            try:
                os.remove(confirm_file)
            except OSError:
                pass
        if not up and http_up(client):
            up = True
            phase("HTTP API снова отвечает")
        if up and confirmed["ok"]:
            return True
        time.sleep(0.4)
    return False


def run_persist(client: Client, stats: Stats, confirm_file: str, resume: bool, expect_ttl: int) -> None:
    if not allow_login():
        stats.skip("сохранение credentials", ALLOW_REASON)
        return
    password = os.environ.get("ESP_CAM_PASSWORD", "")
    if not password:
        stats.skip("сохранение credentials", "нет ESP_CAM_PASSWORD")
        return
    if resume:
        phase("проверка после уже выполненного обновления")
        if not open_operator(client, password):
            stats.fail("миграция: login прежним паролем", "сессия не открыта")
            return
        stats.ok("миграция: login прежним паролем")
        ttl = read_ttl(client)
        if ttl is None or (expect_ttl and ttl != expect_ttl):
            stats.fail("миграция: TTL", f"{ttl} вместо {expect_ttl or 'прочитанного'}")
            return
        stats.ok(f"миграция: TTL совпал ({ttl})")
        token = read_service_token(client)
        if token is None:
            stats.fail("миграция: Service token", "не прочитан")
            return
        client.token = token
        status = bearer_accepted(client, token)
        if status != 500:
            stats.fail("миграция: Bearer", f"HTTP {status}")
            return
        stats.ok("миграция: Bearer принят")
        stats.skip(
            "миграция: Service token совпал с значением до обновления",
            "перезагрузка обновления не была удержана в том же процессе",
        )
    else:
        phase("снимок до обновления прошивки")
        saved = snapshot_credentials(client, stats, password)
        if saved is None:
            return
        token, ttl = saved
        phase("жду пропадание HTTP: обновление прошивки без стирания NVS")
        if not wait_http(client, False, 180, 0):
            stats.fail("миграция: пропадание HTTP", "устройство не перезагрузилось")
            return
        phase("жду HTTP после обновления прошивки")
        if not wait_http(client, True, 90, 1):
            stats.fail("миграция: доступность HTTP", "API не ответил")
            return
        phase("сравнение после обновления прошивки")
        if not compare_saved(client, stats, "миграция", password, token, ttl):
            stats.skip("новый blob: смена пароля", "миграция не подтверждена")
            stats.skip("перезапуск: login прежним паролем", "миграция не подтверждена")
            return
    phase("смена пароля в новом blob и возврат исходного")
    if not roundtrip_password(client, stats, password, token, ttl):
        stats.skip("перезапуск: login прежним паролем", "пароль не возвращён или смена не разрешена")
        return
    phase("жду физический перезапуск питания")
    if not wait_user_power(client, confirm_file):
        stats.fail("перезапуск: подтверждение", "нет пропадания питания, OK или ответа HTTP")
        return
    phase("сравнение после физического перезапуска")
    compare_saved(client, stats, "перезапуск", password, token, ttl)
    stats.skip(
        "отказ записи NVS",
        "нет способа вызвать ошибку NVS без тестового endpoint",
    )


def run_logout(client: Client, stats: Stats) -> None:
    names = (
        "успешный logout",
        "прежняя cookie отклонена",
        "повторный logout без сессии",
    )
    if not allow_login():
        for name in names:
            stats.skip(name, ALLOW_REASON)
        return
    password = os.environ.get("ESP_CAM_PASSWORD", "")
    if not password:
        for name in names:
            stats.skip(name, "нет ESP_CAM_PASSWORD")
        return
    if not open_operator(client, password):
        stats.fail("успешный logout", "новая сессия не открыта")
        stats.skip("прежняя cookie отклонена", "logout не выполнен")
        stats.skip("повторный logout без сессии", "logout не выполнен")
        return
    previous = client.cookie
    reply = client.request("POST", "/api/auth/logout", headers=client.headers("cookie-ui"))
    body = reply.json_body if isinstance(reply.json_body, dict) else None
    data = body.get("data") if isinstance(body, dict) else None
    if (
        not reply.error
        and reply.status == 200
        and isinstance(data, dict)
        and data.get("applied") is True
        and cookie_cleared(reply.set_cookie)
    ):
        stats.ok("успешный logout")
    else:
        stats.fail("успешный logout", f"HTTP {reply.status} сессия не закрыта")
    client.cookie = ""
    probe = client.request(
        "GET",
        "/api/auth/session",
        headers={"X-Esp-Client": UI, "Cookie": f"esp_sess={previous}"},
    )
    expect_probe(stats, "прежняя cookie отклонена", probe, "none")
    again = client.request(
        "POST",
        "/api/auth/logout",
        headers={"X-Esp-Client": UI, "Cookie": f"esp_sess={previous}"},
    )
    expect_error(stats, "повторный logout без сессии", again, 401, "unauthorized", "unauthorized")


def main() -> int:
    parser = argparse.ArgumentParser(description="HTTP-проверки текущего этапа ESP_CAM")
    parser.add_argument("--host", required=True, help="адрес устройства")
    parser.add_argument("--port", type=int, default=80)
    parser.add_argument("--timeout", type=float, default=TIMEOUT_S)
    parser.add_argument(
        "--expire-session",
        action="store_true",
        help="дождаться истечения текущего idle; TTL не меняется",
    )
    parser.add_argument(
        "--expire-nonce",
        action="store_true",
        help="подождать 61 с и проверить, что nonce старше 60 с больше не принимается",
    )
    parser.add_argument(
        "--after-restart",
        action="store_true",
        help="один процесс: снимок, ожидание обновления прошивки, смена пароля, физический перезапуск",
    )
    parser.add_argument(
        "--confirm-file",
        default="",
        help="файл подтверждения физического перезапуска; в него не пишутся token и пароль",
    )
    parser.add_argument(
        "--resume",
        action="store_true",
        help="обновление уже выполнено: проверить миграцию, смену пароля и физический перезапуск",
    )
    parser.add_argument(
        "--expect-ttl",
        type=int,
        default=0,
        help="TTL, прочитанный до обновления; используется вместе с --resume",
    )
    args = parser.parse_args()

    stats = Stats()
    client = Client(args.host, args.port, args.timeout)
    if args.after_restart:
        try:
            run_persist(client, stats, args.confirm_file, args.resume, args.expect_ttl)
        except KeyboardInterrupt:
            print("прервано")
            return 130
        print(f"\nитого: PASS {stats.passed}  FAIL {stats.failed}  SKIP {stats.skipped}")
        return 1 if stats.failed else 0
    try:
        client.open_session(stats)
        run_routes(client, stats)
        run_auth_matrix(client, stats, "plain")
        run_login_body(client, stats)
        run_auth_flow(client, stats)
        run_auth_matrix(client, stats, "cookie")
        run_auth_matrix(client, stats, "bearer")
        run_auth_forms(client, stats)
        run_auth_write(client, stats)
        run_known_stub(client, stats)
        run_status(client, stats)
        run_idle(client, stats, args.expire_session)
        run_logout(client, stats)
        run_framing(client, stats)
        run_legacy(client, stats)
        run_expire_nonce(client, stats, args.expire_nonce)
        run_after_restart(client, stats, args.after_restart)
    except KeyboardInterrupt:
        print("прервано")
        return 130

    print(
        f"\nитого: PASS {stats.passed}  FAIL {stats.failed}  SKIP {stats.skipped}"
    )
    return 1 if stats.failed else 0


if __name__ == "__main__":
    sys.exit(main())
