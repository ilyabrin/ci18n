# Performance

Measured with `make bench` on an Intel Core i5-12400F, gcc 13 at `-O2`,
Linux, 1 000-key Russian catalogue with English as fallback. Your numbers
will differ; see [bench/README.md](../bench/README.md) for the method and how
to run it.

| Operation | Time |
| --- | ---: |
| `ci18n_get`, key found | 24 ns |
| `ci18n_get`, found in the fallback language | 38 ns |
| `ci18n_get`, key missing | 41 ns |
| `ci18n_format_plural` | 91 ns |
| `ci18n_format`, two placeholders | 104 ns |
| `ci18n_format`, value through a formatter | 170 ns |
| Load 1 000 keys | 0.17 ms |
| Load 10 000 keys | 1.5 ms |

For reference, glibc's `gettext` on the same keys and machine takes 133 ns
for a found key and 679 ns for a missing one (`make bench-gettext`).

With threads, reads scale with cores: one thread does 27 million `ci18n_get`
calls a second and eight threads 162 million between them
(`make bench-threads`); see
[Catalogues and threads](catalogs-and-threads.md#the-shared-mode).

For code size and memory, see [Embedded and size](embedded-and-size.md).
