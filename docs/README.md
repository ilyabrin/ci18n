# ci18n documentation

Start with [Getting started](getting-started.md), then go to whatever your
program needs next.

| Page | Read it when you want to |
| --- | --- |
| [Getting started](getting-started.md) | Load translations, look them up, handle errors, pick the user's language |
| [Translations](translations.md) | Write translation files: escapes, plurals, ordinals |
| [Formatting](formatting.md) | Put names, counts, dates and numbers into translations |
| [Unicode and text direction](unicode-and-direction.md) | Count and cut UTF-8, support Arabic and Hebrew, mix directions |
| [Catalogues and threads](catalogs-and-threads.md) | Use ci18n inside a library, or from several threads |
| [Embedded and size](embedded-and-size.md) | Make it smaller, set limits, run it on a microcontroller |
| [Coming from gettext](from-gettext.md) | Load `.mo` files, or convert `.po` files once |
| [API reference](api.md) | Find a function, or an error code |
| [Performance](performance.md) | Know what a lookup, a format or a load costs |
| [Platforms](platforms.md) | Check what CI builds and tests on |
| [Scope](scope.md) | Know what ci18n does not do, and what to use instead |

Working code for three realistic programs is in [examples/](../examples/), and
every function is documented once more next to its declaration in
[include/ci18n.h](../include/ci18n.h).
