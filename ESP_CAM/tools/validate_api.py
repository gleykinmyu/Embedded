#!/usr/bin/env python3
"""Validate api/openapi.yaml as OpenAPI 3.1. Does not build or modify firmware."""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    spec_path = root / "api" / "openapi.yaml"
    try:
        from openapi_spec_validator import validate
        from openapi_spec_validator.readers import read_from_filename
    except ImportError:
        print(
            "Install the checker: python -m pip install -r tools/requirements-api.txt",
            file=sys.stderr,
        )
        return 2

    spec, _base = read_from_filename(str(spec_path))
    version = spec.get("openapi")
    if version != "3.1.0":
        print(f"expected OpenAPI 3.1.0, found {version}", file=sys.stderr)
        return 1
    validate(spec)
    paths = spec.get("paths") or {}
    methods = {"get", "post", "put", "patch", "delete"}
    operations = 0
    for item in paths.values():
        if isinstance(item, dict):
            operations += sum(1 for key in item if key in methods)
    print(f"valid OpenAPI {version}: {len(paths)} paths, {operations} operations")
    return 0


if __name__ == "__main__":
    sys.exit(main())
