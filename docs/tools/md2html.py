#!/usr/bin/env python3
"""Convert a user manual (Markdown) to a print-ready HTML file.

Usage: md2html.py input.md output.html
Requires the 'markdown' package (pip install markdown).
"""
import re
import sys
import unicodedata

import markdown


def github_slug(value, separator="-"):
    """Same anchors as GitHub, so the manual's internal links work in both places."""
    value = unicodedata.normalize("NFC", value).strip().lower()
    value = re.sub(r"[^\w\- ]", "", value)
    return value.replace(" ", separator)


CSS = """
@page { size: A4; margin: 18mm 16mm 18mm 16mm; }
html { -webkit-print-color-adjust: exact; }
body { font-family: -apple-system, "Helvetica Neue", Helvetica, Arial, sans-serif; font-size: 10.5pt;
       line-height: 1.45; color: #1d2330; margin: 0; }
.cover { text-align: center; padding-top: 60mm; page-break-after: always; }
.cover img { height: 80px; }
.cover h1 { font-size: 30pt; margin: 18mm 0 4mm 0; border: none; }
.cover .sub { font-size: 13pt; color: #5b6475; }
h1 { font-size: 20pt; color: #172036; border-bottom: 2px solid #2f7bf0; padding-bottom: 4px; }
h2 { font-size: 15pt; color: #172036; border-bottom: 1px solid #d5dbe6; padding-bottom: 3px;
     margin-top: 22px; page-break-after: avoid; }
h2.chapter { page-break-before: always; }
h3 { font-size: 12pt; color: #23314f; margin-top: 16px; page-break-after: avoid; }
h4 { font-size: 11pt; color: #23314f; page-break-after: avoid; }
a { color: #1f63c9; text-decoration: none; }
code { font-family: Menlo, Monaco, monospace; font-size: 9pt; background: #eef1f6; padding: 1px 3px;
       border-radius: 3px; }
pre { background: #f3f5f9; border: 1px solid #dde2ea; border-radius: 5px; padding: 8px 10px;
      overflow: hidden; page-break-inside: avoid; white-space: pre; }
pre code { background: none; padding: 0; font-size: 8.4pt; line-height: 1.3; }
table { border-collapse: collapse; width: 100%; margin: 8px 0 12px 0; page-break-inside: auto; }
tr { page-break-inside: avoid; }
th, td { border: 1px solid #d5dbe6; padding: 4px 7px; text-align: left; vertical-align: top; }
th { background: #e9eef7; }
tr:nth-child(even) td { background: #f8f9fc; }
blockquote { margin: 10px 0; padding: 6px 12px; border-left: 4px solid #2f7bf0; background: #f1f6fe;
             color: #33405c; }
hr { display: none; }
"""


def nest_lists(lines):
    """GitHub accepts 2-space indented sub-lists, Python-Markdown needs 4: double the indent."""
    out, in_code = [], False
    for line in lines:
        if line.lstrip().startswith("```"):
            in_code = not in_code
        if not in_code and line.startswith("  ") and line.strip():
            stripped = line.lstrip(" ")
            line = " " * ((len(line) - len(stripped)) * 2) + stripped
        out.append(line)
    return out


def fit_code_blocks(html):
    """Shrink the font of wide code blocks (diagrams) so lines never wrap on an A4 page."""
    import html as htmlmod

    def repl(m):
        text = htmlmod.unescape(re.sub(r"<[^>]+>", "", m.group(2)))
        longest = max((len(l) for l in text.splitlines()), default=0)
        size = 8.4 if longest == 0 else min(8.4, 455.0 / (0.61 * longest))
        return f'<pre><code{m.group(1)} style="font-size:{size:.2f}pt">{m.group(2)}</code></pre>'

    return re.sub(r"<pre><code([^>]*)>(.*?)</code></pre>", repl, html, flags=re.S)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    text = open(src, encoding="utf-8").read()

    # Links to the other language point at the PDF version.
    text = re.sub(r"\((Manual-[a-z]+)\.md\)", r"(\1.pdf)", text)

    # Pull the title and subtitle out for a cover page.
    lines = text.splitlines()
    title = lines[0].lstrip("# ").strip()
    subtitle = ""
    body_start = 1
    for i in range(1, min(len(lines), 6)):
        if lines[i].startswith("*") and lines[i].endswith("*"):
            subtitle = lines[i].strip("*")
            body_start = i + 1
            break
    body_md = "\n".join(nest_lists(lines[body_start:]))

    html_body = markdown.markdown(
        body_md,
        extensions=["tables", "fenced_code", "toc", "sane_lists"],
        extension_configs={"toc": {"slugify": github_slug}},
        output_format="html5",
    )
    html_body = fit_code_blocks(html_body)
    # Every numbered chapter and the appendix starts on a new page.
    html_body = re.sub(r'<h2 id="(\d+-|appendi)', r'<h2 class="chapter" id="\1', html_body)

    subtitle_html = markdown.markdown(subtitle).replace("<p>", "").replace("</p>", "")
    html = f"""<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>{title}</title><style>{CSS}</style></head>
<body>
<div class="cover">
  <img src="../resources/logo_full.png" alt="Artilody">
  <h1>{title}</h1>
  <div class="sub">{subtitle_html}</div>
</div>
{html_body}
</body></html>
"""
    open(dst, "w", encoding="utf-8").write(html)


if __name__ == "__main__":
    main()
