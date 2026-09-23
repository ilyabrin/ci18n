# Benchmarks

Three programs, each printing a Markdown table you can paste as is.

| Command | Measures | Needs |
| --- | --- | --- |
| `make bench` | Lookups, formatting, loading, memory | Any C99 compiler |
| `make bench-threads` | One catalogue shared by 1 to 8 threads | pthreads |
| `make bench-gettext` | The same lookups through glibc gettext | Linux, `msgfmt`, `ru_RU.UTF-8` |

With CMake, `bench.c` builds as `ci18n_bench`. Time a Release build:

```sh
cmake -B build && cmake --build build --config Release
./build/ci18n_bench            # build/Release/ci18n_bench.exe with Visual Studio
```

For the gettext comparison on Debian or Ubuntu:

```sh
sudo apt-get install gettext locales
sudo locale-gen ru_RU.UTF-8
make bench-gettext
```

## How it measures

- Each number is the median of 7 rounds. A round runs long enough, about
  100 ms, that clock resolution does not matter.
- Lookups use a 1 000-key Russian catalogue with English as the fallback.
  Keys are visited in shuffled order, so the numbers do not come from a
  warm, predictable cache line.
- The build is the default one with one exception: the key cap is raised to
  16 384 so the 10 000-key catalogue fits. The default cap is 1 024; see
  `CI18N_MAX_KEYS_PER_LANGUAGE`.
- "In memory" is what the library itself allocates: string arenas, entry
  arrays, hash buckets, and the catalogue struct. Each language has three heap
  blocks, so allocator overhead adds only a few dozen bytes.

## Reading the results

- **Timings are relative to your machine.** Compare numbers from the same
  run, not numbers from different machines.
- **CI builds the benchmarks but never times them.** Shared runners are too
  noisy for timing, and building everywhere keeps the code compiling.
- **Memory is about 1.4 times the size of the catalogue file.** Buffers grow
  by doubling while a file loads, and the loader gives the spare room back at
  the end. The rest is the entry array and the hash buckets.
- **gettext pays for locale checks on every call.** It reads the environment
  and the current locale each time, which is the price its callers pay, so the
  comparison is fair to what an application sees. gettext also maps the `.mo`
  file instead of copying it, so it uses less heap.
