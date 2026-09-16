# Verification record — 2026-09-16

Executed in the delivery environment:

- Compiler: GCC 13.3.0, Linux x86_64, `-std=c++23`.
- Warnings: `-Wall -Wextra -Wpedantic -Werror`.
- Sanitizers: address and undefined behavior, frame pointers enabled.
- LeakSanitizer unavailable: process inspection restricted; rerun with `ASAN_OPTIONS=detect_leaks=0`.
- Shared behavioral suite: **22 cases, 0 failures**. Actual stdout retained in results/core-tests.txt.
- Project XML and CMake presets JSON parsed; referenced source/resource paths checked.

Not executed in the delivery environment:

- CMake configuration/build (CMake was not installed here).
- MSVC v145 / Visual Studio 2026 build.
- Qt/OCCT linking, all AdapterTests, rendering, HiDPI and picking.
- SQL Server schema/procedures, QODBC connection or concurrent users.
- Windows dependency installation, deployment, packaging/installer, generated report visual QA.
- GitHub Actions: workflow source supplied, never claimed as a passing CI run.

Dependency-free reproduction:

```bash
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror -Iinclude src/core/Domain.cpp tests/Standalone.cpp -o core-tests
./core-tests
```

With CMake/Ninja installed:

```bash
cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only
```

Windows full tests are integrated into scripts/build.cmd. AdapterTests independently derives
volume/area expectations for analytic solids, checks malformed data and hash changes,
and verifies document round trips and HTML escaping. No precomputed image is passed off as a UI run.
