## What this changes

<!-- One or two sentences. If it fixes an issue, link it. -->

## Why

<!-- What was wrong, or what became possible. -->

## Checklist

- [ ] `make clean && make EXTRA_CFLAGS=-Werror` passes
- [ ] A test covers the change, if it touches the parser or the lookup path
- [ ] Public API changes are documented in the header and in `README.md`
- [ ] `CI18N_VERSION_*` bumped, if a signature or a behaviour changed
- [ ] A fuzz seed added, if this fixes a parser bug

CI also sweeps c99, c11 and c17, runs the sanitizers and fuzzes the parser, so
those do not need running by hand. `CONTRIBUTING.md` has the commands if you
want them locally.
