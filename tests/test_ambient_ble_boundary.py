#!/usr/bin/env python3
"""Keep the donor transport extraction behind a project-owned byte API."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
COMPONENT = ROOT / "components" / "ambient_ble"


def fail(message: str) -> None:
    print(f"R3 BLE boundary: {message}", file=sys.stderr)
    raise SystemExit(1)


sources = [p for p in COMPONENT.rglob("*") if p.is_file() and p.suffix in {".c", ".h"}]
forbidden = re.compile(
    r"esp_desktop_buddy|buddy_transport_ble|BUDDY_NUS|\btx_ready\b|"
    r"buddy_(?:core|message|event|snapshot|quota)|permission_(?:grant|approve)",
    re.IGNORECASE,
)
for source in sources:
    contents = source.read_text(encoding="utf-8")
    code = re.sub(r"/\*.*?\*/|//[^\n]*", " ", contents, flags=re.DOTALL)
    if re.search(r"gen_epoch|next_session_generation|ambient_generation", code):
        fail("Passport transport retains generation allocation ownership")
    if forbidden.search(code):
        fail(f"donor semantic/security surface leaked into {source.relative_to(ROOT)}")

header = (COMPONENT / "include" / "ambient_ble.h").read_text(encoding="utf-8")
if "const uint8_t *frame" not in header or "uint8_t *out" not in header:
    fail("public transport API is not byte-only")
if "ambient_wire" in header or "ambient_wire" in "\n".join(p.read_text() for p in sources):
    fail("BLE layer depends on the R1 semantic model")

cmake = (COMPONENT / "CMakeLists.txt").read_text(encoding="utf-8")
if not re.search(r"REQUIRES\s+bt\s+nvs_flash", cmake):
    fail("component must depend on the platform NimBLE and NVS APIs only")
if "esp_desktop_buddy" in cmake.lower():
    fail("component retains a donor build dependency")

license_text = (COMPONENT / "LICENSE").read_text(encoding="utf-8")
if "Apache License" not in license_text or "Version 2.0" not in license_text:
    fail("pinned donor license was not preserved")
notice = (COMPONENT / "NOTICE").read_text(encoding="utf-8")
for expected in ("b6bac05db208717676e70180e5269d79f32b2d68", "Apache-2.0", "No Buddy core"):
    if expected not in notice:
        fail(f"provenance notice missing {expected!r}")

print("R3 BLE boundary: PASS")
