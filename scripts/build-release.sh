#!/usr/bin/env sh
set -eu

# Stage 1: Build the native static server automatically and collect any
# additional cross-compiled servers already staged in prebuilt/.
python -m build

# Stage 2: Validate the completed PyPI artifacts.
python -m twine check dist/*
