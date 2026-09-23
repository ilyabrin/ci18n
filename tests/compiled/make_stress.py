#!/usr/bin/env python3
"""
Write tests/compiled/stress.txt, a translation file with every corner of the
format in it, for the test that compares compiled and loaded languages.

Generated rather than written by hand because the corners are bytes an
editor or a checkout would quietly change: a BOM, CRLF, lone CRs. The file is
marked binary in .gitattributes for the same reason.

SPDX-License-Identifier: MIT
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))

CRLF_PART = [
    r"# Every corner of the format, for the compiled-versus-loaded test.",
    r"greeting=Hello, {name}!",
    r"  spaced   =   trimmed on both sides   ",
    r"empty=",
    r"escapes=tab\there\nnew line \\ backslash \= equals",
    r"we\=ird=a key with an equals sign",
    r"\#hash=a key starting with a comment marker",
    r"trailing=keeps one space\ ",
    r"menu.open=Open",
    "unicode=Привет مرحبا "
    "你好 \U0001F600",
]
CR_PART = [
    r"duplicate=first",
    r"duplicate=second wins",
    r"files[one]={count} file",
    r"files[other]={count} files",
]
LF_PART = [
    r"place[one]={count}st",
    r"place[two]={count}nd",
    r"place[few]={count}rd",
    r"place[other]={count}th",
    r"bare=no forms at all",
    r"; also a comment",
]

body = ("\r\n".join(CRLF_PART) + "\r\n" + "\r".join(CR_PART) + "\r" +
        "\n".join(LF_PART) + "\n")

with open(os.path.join(HERE, "stress.txt"), "wb") as f:
    f.write(b"\xef\xbb\xbf" + body.encode("utf-8"))
