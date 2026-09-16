# Third-party dependencies and references

The supplied archive contains project source and original user-provided icons/resources,
not Qt/OCCT/GoogleTest binaries. License notices for downloaded packages are supplied by
vcpkg under installed/x64-windows/share/<port>/copyright; preserve relevant notices when packaging.
Review the terms of the exact Qt modules and OCCT release selected before redistribution.
This file is an inventory, not a substitute for their licenses.

| Dependency | Use |
|---|---|
| Qt 6 Core/Gui/Widgets/Concurrent/Sql/PrintSupport | Desktop, async import, ODBC, PDF output |
| Open CASCADE Technology 7.x | CAD transfer, B-Rep geometry, visualization |
| GoogleTest | Test harness, not shipped as application feature |
| CMake / Ninja / vcpkg | Build and dependency management |
| Microsoft ODBC Driver 18 | SQL Server connectivity, installed separately |
| MSVC / Windows SDK | Target toolchain |
| NSIS | Optional packaging recipe |

Official technical sources consulted:

- [Visual Studio CMake projects](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170)
- [Visual Studio 18 2026 CMake generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html)
- [Qt SQL drivers / ODBC](https://doc.qt.io/qt-6/sql-driver.html)
- [OCCT STEPControl_Reader](https://occt3d.com/dev/doc/refman/html/class_s_t_e_p_control___reader.html)
- [Microsoft vcpkg Nov 2025–Jan 2026 release notes](https://devblogs.microsoft.com/cppblog/whats-new-in-vcpkg-nov-2025-jan-2026/)
- [Qt port features](https://vcpkg.io/en/package/qtbase.html)
- [OCCT port versions](https://vcpkg.io/en/package/opencascade.html)

The default dependency registry tag is 2025.12.12 to avoid automatically migrating to OCCT 8.
The setup script verifies the selected OCCT major and writes the actual commit/package list.
That setup has not been executed in this Linux delivery environment; the lock is generated
on the first Windows setup, not fabricated in this archive.
