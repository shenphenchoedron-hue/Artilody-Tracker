#!/bin/sh
# Regenerates docs/Manual-da.pdf and docs/Manual-en.pdf from the Markdown manuals.
# macOS only (uses WebKit through Swift). Needs Python 3 with the 'markdown' package:
#   python3 -m pip install markdown
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-python3}"

for lang in da en; do
    "$PY" tools/md2html.py "Manual-$lang.md" "Manual-$lang.html"
    swift tools/html2pdf.swift "Manual-$lang.html" "Manual-$lang.pdf"
    rm -f "Manual-$lang.html"
done
