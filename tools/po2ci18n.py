#!/usr/bin/env python3
"""Convert a gettext .po catalogue into a ci18n translation file.

    tools/po2ci18n.py ru.po > translations/ru.txt
    tools/po2ci18n.py --keys=msgid ru.po -o translations/ru.txt

Why a converter rather than a .po reader inside the library: a full reader is
around 700 lines, 300 of them an evaluator for the C expression gettext puts in
its Plural-Forms header. That doubles the parse and fuzz surface of a library
whose point is one small header, for a format almost everyone converts once at
build time. Doing it here also means the expression is evaluated on a
development machine rather than on a device, and that keys can be renamed from
English sentences into identifiers.

What it handles:

  - msgid and msgstr, including C-style escapes and the adjacent-string
    continuation gettext uses for long entries
  - msgid_plural with msgstr[0], msgstr[1] and so on, mapped onto CLDR
    category names using the file's own Plural-Forms rule
  - msgctxt, as a key prefix
  - skips fuzzy entries, which are unreviewed, and obsolete ones
  - skips untranslated entries rather than emitting empty values

SPDX-License-Identifier: MIT
"""

import argparse
import re
import sys

# CLDR category order per nplurals count, for the common rule shapes. gettext
# gives us an index; ci18n wants a name. The mapping depends on the language's
# rule, and these cover what the Plural-Forms expressions in the wild produce.
CATEGORY_ORDERS = {
    1: ["other"],
    2: ["one", "other"],
    3: ["one", "few", "many"],
    4: ["one", "few", "many", "other"],
    5: ["zero", "one", "two", "few", "many"],
    6: ["zero", "one", "two", "few", "many", "other"],
}

ESCAPES = {
    "n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\",
    "a": "\a", "b": "\b", "f": "\f", "v": "\v",
}

# Everything ci18n's own format treats specially, so a converted value cannot
# accidentally become a comment, a separator or a swallowed space.
CI18N_ESCAPES = {
    "\\": "\\\\", "\n": "\\n", "\t": "\\t", "\r": "\\r", "=": "\\=",
}


def unquote(text):
    """Decode one C-style quoted string from a .po file."""
    out = []
    i = 0
    while i < len(text):
        c = text[i]
        if c == "\\" and i + 1 < len(text):
            out.append(ESCAPES.get(text[i + 1], "\\" + text[i + 1]))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def escape_for_ci18n(value, is_key):
    """Escape a decoded string so ci18n's parser reads it back unchanged."""
    out = []
    for c in value:
        out.append(CI18N_ESCAPES.get(c, c))
    text = "".join(out)

    if is_key:
        # A key may not start with a comment marker unless it is escaped.
        if text.startswith("#") or text.startswith(";"):
            text = "\\" + text
    else:
        # A trailing space would be trimmed away, so mark it as content.
        if text.endswith(" "):
            text = text[:-1] + "\\ "

    return text


def parse_po(lines):
    """Yield entries as dicts. Deliberately line oriented: .po is a simple
    format and a tokenizer would not earn its keep here."""
    entry = {"flags": [], "msgstr": {}}
    field = None

    def flush():
        nonlocal entry, field
        if entry.get("msgid") is not None or entry["msgstr"]:
            yielded = entry
            entry = {"flags": [], "msgstr": {}}
            field = None
            return yielded
        entry = {"flags": [], "msgstr": {}}
        field = None
        return None

    for raw in lines:
        line = raw.strip()

        if not line:
            done = flush()
            if done:
                yield done
            continue

        if line.startswith("#~"):
            # Obsolete entry, deliberately dropped.
            entry["obsolete"] = True
            continue

        if line.startswith("#,"):
            entry["flags"] = [f.strip() for f in line[2:].split(",")]
            continue

        if line.startswith("#"):
            continue

        match = re.match(r'^(msgctxt|msgid_plural|msgid|msgstr(?:\[(\d+)\])?)\s+"(.*)"$',
                         line)
        if match:
            name = match.group(1)
            index = match.group(2)
            value = unquote(match.group(3))

            if name.startswith("msgstr"):
                key = int(index) if index is not None else 0
                entry["msgstr"][key] = value
                field = ("msgstr", key)
            else:
                entry[name] = value
                field = (name, None)
            continue

        # A bare quoted string continues the previous field, which is how
        # gettext wraps long entries.
        match = re.match(r'^"(.*)"$', line)
        if match and field:
            value = unquote(match.group(1))
            name, index = field
            if name == "msgstr":
                entry["msgstr"][index] += value
            else:
                entry[name] += value

    done = flush()
    if done:
        yield done


def plural_categories(header_value, count):
    """Map msgstr indices onto CLDR category names.

    The Plural-Forms expression decides how many forms there are and in which
    order; the names come from the CLDR order for that count. Evaluating the
    expression itself is not needed for conversion, only its nplurals.
    """
    nplurals = count
    if header_value:
        match = re.search(r"nplurals\s*=\s*(\d+)", header_value)
        if match:
            nplurals = int(match.group(1))

    order = CATEGORY_ORDERS.get(nplurals)
    if not order:
        order = ["other"] * nplurals
    return order


def convert(lines, key_mode, out):
    header = ""
    written = 0
    skipped_fuzzy = 0
    skipped_empty = 0

    entries = list(parse_po(lines))

    for entry in entries:
        if entry.get("msgid") == "" and entry["msgstr"].get(0):
            header = entry["msgstr"][0]
            break

    plural_forms = ""
    match = re.search(r"Plural-Forms:([^\n]*)", header)
    if match:
        plural_forms = match.group(1)

    out.write("# Converted from a gettext catalogue by tools/po2ci18n.py\n")
    out.write("# Format: key=value, # for comments\n\n")

    for entry in entries:
        msgid = entry.get("msgid")

        if msgid is None or msgid == "" or entry.get("obsolete"):
            continue

        if "fuzzy" in entry["flags"]:
            # Unreviewed translations. Emitting them would present drafts as
            # finished work.
            skipped_fuzzy += 1
            continue

        key = msgid
        if entry.get("msgctxt"):
            key = entry["msgctxt"] + "." + key

        if key_mode == "slug":
            slug = re.sub(r"[^A-Za-z0-9]+", "_", key).strip("_").lower()
            key = slug[:64] if slug else key

        if entry.get("msgid_plural") is not None:
            categories = plural_categories(plural_forms, len(entry["msgstr"]))
            any_written = False

            for index in sorted(entry["msgstr"]):
                value = entry["msgstr"][index]
                if not value:
                    continue
                if index >= len(categories):
                    continue

                out.write("%s[%s]=%s\n" % (escape_for_ci18n(key, True),
                                           categories[index],
                                           escape_for_ci18n(value, False)))
                any_written = True

            if any_written:
                written += 1
            else:
                skipped_empty += 1
            continue

        value = entry["msgstr"].get(0, "")
        if not value:
            skipped_empty += 1
            continue

        out.write("%s=%s\n" % (escape_for_ci18n(key, True),
                               escape_for_ci18n(value, False)))
        written += 1

    return written, skipped_fuzzy, skipped_empty


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("input", help="the .po file to convert")
    parser.add_argument("-o", "--output", help="write here instead of stdout")
    parser.add_argument("--keys", choices=["msgid", "slug"], default="msgid",
                        help="msgid keeps the English string as the key; slug "
                             "turns it into a lowercase identifier, which is "
                             "shorter and avoids the key length limit")
    args = parser.parse_args()

    with open(args.input, encoding="utf-8") as f:
        lines = f.readlines()

    if args.output:
        with open(args.output, "w", encoding="utf-8", newline="\n") as out:
            stats = convert(lines, args.keys, out)
    else:
        stats = convert(lines, args.keys, sys.stdout)

    written, fuzzy, empty = stats
    sys.stderr.write("%d entries written, %d fuzzy skipped, %d untranslated "
                     "skipped\n" % (written, fuzzy, empty))
    return 0


if __name__ == "__main__":
    sys.exit(main())
