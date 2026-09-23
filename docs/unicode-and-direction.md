# Unicode and text direction

The library stores and returns bytes, so UTF-8 passes through untouched. This
page is about the places where bytes are not enough: counting and cutting
text, languages written right to left, and sentences that mix the two.

- [UTF-8 helpers](#utf-8-helpers)
- [Text direction](#text-direction)
- [Mixed-direction text](#mixed-direction-text)

## UTF-8 helpers

Counting characters is not counting bytes:

```c
strlen("Привет");            /* 12 */
ci18n_utf8_length("Привет");  /* 6  */
```

And cutting a string to fit a buffer can land in the middle of a character,
which produces bytes no decoder accepts. `ci18n_utf8_truncate` cuts only at a
boundary:

```c
char label[16];

ci18n_get_copy("title", label, sizeof(label));
ci18n_utf8_truncate(label, sizeof(label) - 1);
```

The budget is the text length, terminator not counted, so a `char[16]` takes
`sizeof(buffer) - 1`. The result can come out up to three bytes short of the
budget, because the cut moves back rather than splitting a character. Nothing
is appended, so if you want an ellipsis there is room for one.

Truncate before copying into a smaller buffer, not after: a copy cut at the
last byte already fits, so there is nothing left for `ci18n_utf8_truncate` to
repair. [examples/embedded_ui](../examples/embedded_ui/) shows the right order.

`ci18n_format` and its relatives already do this for you; see
[Formatting](formatting.md#buffers-and-truncation).

Validation is strict, in the sense the Unicode standard requires:

```c
ci18n_utf8_valid(text);
```

It rejects what is not UTF-8, rather than only what fails to decode: a
character written in more bytes than it needs, a surrogate half from U+D800
to U+DFFF, anything above U+10FFFF, a continuation byte with no lead byte,
and a sequence the string ends in the middle of. Lenient decoders are how a
check on one representation gets bypassed with another. Check text that
comes from outside, such as a language pack received over the network,
before loading it.

To walk a string a character at a time:

```c
for (const char *p = text; *p; p += ci18n_utf8_sequence_length(p))
{
    /* p points at one whole character */
}
```

That never stalls: an invalid byte reports 1, stepping past the problem
instead of looping forever on it.

One honest limit. "Character" here means one Unicode codepoint, which is the
useful answer for a length limit but still not the number of things a reader
would count. An accent written as a separate combining mark is its own
codepoint, and a single emoji can be several. Counting those needs grapheme
clusters, which needs the Unicode character database, which is exactly the
kind of data this library does not ship.

## Text direction

Arabic, Hebrew and Persian are written right to left, and an interface has to
know which way round to put things. The direction is a property of the
language, so the library answers that; the layout is yours.

```c
ci18n_set_current("ar");

ci18n_current_direction();                        /* CI18N_DIR_RTL */
ci18n_direction_name(ci18n_current_direction());  /* "rtl" */
```

The names are the ones HTML and CSS use, so they drop straight in:

```c
printf("<html lang=\"%s\" dir=\"%s\">\n",
       ci18n_get_current(),
       ci18n_direction_name(ci18n_current_direction()));
```

`ci18n_direction(code)` answers for any code without touching the current
language. A script subtag decides on its own, because direction belongs to
the script rather than the language:

| Code | Direction | Why |
| --- | --- | --- |
| `ar`, `ar-EG`, `ar_EG.UTF-8` | rtl | Arabic, region and charset ignored |
| `he`, `fa`, `ur`, `ps`, `dv`, `yi`, `ckb` | rtl | default script is right to left |
| `ku`, `az`, `pa` | ltr | Latin or Gurmukhi unless told otherwise |
| `az-Arab`, `pa-Arab`, `ku-Arab` | rtl | the script subtag overrides |
| `ar-Latn` | ltr | romanized Arabic reads left to right |
| `xx`, `""`, `NULL` | ltr | unknown, and left to right is the safe guess |

An unknown language is left to right. That is the commoner answer and the
safer one: a left-to-right interface shown right to left is broken in a way
nobody misses, while the reverse merely looks untranslated.

## Mixed-direction text

A value in the other direction reorders the sentence around it. An English
name inside Arabic drags the comma after it to the wrong side, and a Hebrew
name in an English sentence can swap places with the number beside it. The
fix is to isolate each value, so the renderer lays it out on its own:

```c
ci18n_set_bidi_isolation(true);
ci18n_format(out, sizeof(out), "inbox", "name", "Sam", NULL);
/* "{name}، لديك..." becomes FSI "Sam" PDI "، لديك..." */
```

Every value `ci18n_format` and its relatives fill in, `{count}` and
formatter output included, is wrapped in FSI and PDI, two invisible
characters that browsers, GTK, Qt, Android and iOS all honour. Text the
translator wrote is never wrapped. The setting is per catalogue and off by
default: turn it on for text people read, and leave it off for logs and
anything a program parses, since the marks are real bytes, 3 each.

| For | Use |
| --- | --- |
| A value placed without `ci18n_format` | `ci18n_bidi_isolate(out, cap, text)` |
| A line ending in a number or a symbol | append `ci18n_bidi_mark(dir)`, LRM or RLM |
| The raw characters | `CI18N_FSI`, `CI18N_PDI`, `CI18N_LRM`, `CI18N_RLM` |

This marks text for a renderer that runs the Unicode bidirectional
algorithm. It does not run the algorithm itself, so a display controller that
draws bytes left to right as they come still needs a bidi step of its own.
