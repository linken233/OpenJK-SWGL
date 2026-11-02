"""Packs docs/phys/pk3/ into Z_SWGL_Phys.pk3, with forward slashes in the paths.

The help text of the editor (ui/physhelp.txt) is made from docs/phys-format.md.
"""
import os
import zipfile

here = os.path.dirname(os.path.abspath(__file__))
src = os.path.join(here, "pk3")
out = os.path.join(here, "Z_SWGL_Phys.pk3")
doc = os.path.join(here, "..", "phys-format.md")


def markdown_to_text(md):
    lines = []
    kind = "other"      # kind of the last line: para, bullet or other
    in_code = False

    for raw in md.splitlines():
        line = raw.rstrip()

        if line.startswith("```"):
            in_code = not in_code
            kind = "other"
            continue

        if in_code:
            lines.append("    " + line.expandtabs(4))
            kind = "other"
            continue

        if line.startswith("|"):
            cells = [c.strip().replace("`", "") for c in line.strip("|").split("|")]
            kind = "other"

            if all(set(c) <= set("-: ") for c in cells):
                continue

            if cells[0] in ("Keyword", "Rule", "Line", "Name"):
                continue

            if len(cells) == 3:
                lines.append("%s  (default %s)" % (cells[0], cells[1]))
                lines.append("    " + cells[2])
            else:
                lines.append(cells[0])
                lines.append("    " + " ".join(cells[1:]))

            continue

        line = line.replace("`", "").replace("**", "")

        if line.startswith("# "):
            lines.append(line[2:].upper())
            kind = "other"
        elif line.startswith("## "):
            lines.append("")
            lines.append("== " + line[3:] + " ==")
            kind = "other"
        elif line.startswith("### "):
            lines.append("")
            lines.append("-- " + line[4:] + " --")
            kind = "other"
        elif line == "":
            lines.append("")
            kind = "other"
        elif line.startswith("- "):
            lines.append(line)
            kind = "bullet"
        elif line.startswith("  ") and kind == "bullet":
            lines[-1] += " " + line.strip()
        elif kind == "para":
            lines[-1] += " " + line
        else:
            lines.append(line)
            kind = "para"

    return "\n".join(lines) + "\n"


with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for root, _, files in os.walk(src):
        for name in sorted(files):
            full = os.path.join(root, name)
            z.write(full, os.path.relpath(full, src).replace(os.sep, "/"))
            print(os.path.relpath(full, src).replace(os.sep, "/"))

    with open(doc, encoding="utf-8") as f:
        z.writestr("ui/physhelp.txt", markdown_to_text(f.read()))
        print("ui/physhelp.txt")
