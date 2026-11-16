# TITAN-RT-001 — SPSC R02 reference implementation

This repository contains a C++17 fixed-capacity SPSC queue and a dependency-free
executable test suite. It is a reference implementation, **not a production
real-time certification**.

## Contract

- Exactly one producer calls `push()` and exactly one consumer calls `pop()`.
- Operations are bounded and do not allocate internally. `push()` fails when
  full; the caller must apply and observe an explicit drop policy.
- `T` is a trivially copyable, trivially default-constructible record.
- The target must provide always-lock-free atomics for the selected unsigned
  counter type. This is enforced at compile time.
- `size_approx()` and `empty_approx()` are diagnostics, not synchronization
  primitives. Concurrent snapshots may be approximate.
- `alignas(64)` is a deployment assumption and does not prove that every target
  uses a 64-byte destructive-interference size.
- Lock-free atomic support alone does not establish wait-free behavior or meet
  an audio callback deadline.

The optional counter template parameter exists to make unsigned wraparound
reachable in tests. Production instantiations should use the default
`std::size_t` counter unless a target-specific design review selects otherwise.

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

ThreadSanitizer build (supported GCC/Clang platforms):

```sh
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DTITAN_ENABLE_TSAN=ON
cmake --build build-tsan --parallel
ctest --test-dir build-tsan --output-on-failure
```

Sanitizer results, x86-64/ARM64 builds, and callback deadline measurements must
be collected on their respective target environments before claiming those
acceptance criteria are met. This submission does not claim those executions
have occurred.
