# CodeAsMetal — V1 Engineering Workbench

> Engineering intelligence for connecting CAD geometry, manufacturing decisions,
> process planning, review, and should-cost analysis.

**Version:** `1.0.0-preview.1`  
**Platform:** Windows x64  
**Language:** C++23  
**UI:** Qt 6  
**CAD Kernel:** OpenCASCADE  
**Build:** CMake + Ninja + Visual Studio 2026  
**Dependencies:** vcpkg

CodeAsMetal is an engineering workbench for turning CAD geometry into structured
manufacturing information.

The V1 architecture connects:

```text
                           CODEASMETAL
                              C++23
                                │
                     ┌──────────┴──────────┐
                     │    APPLICATION      │
                     └──────────┬──────────┘
                                │
         ┌──────────────────────┼──────────────────────┐
         │                      │                      │
 PRODUCT DEFINITION       MANUFACTURING          ENGINEERING REVIEW
         │                      │                      │
   Part / Revision          Process Plan              Issue
      Geometry              Operations             Annotation
   Requirements             Resources              Decision
   Drawing / PMI            Inspection             Override
         │                      │                      │
         └──────────────────────┼──────────────────────┘
                                │
                         ENGINEERING DATA
                                │
             ┌──────────────────┼──────────────────┐
             │                  │                  │
            DFM             SHOULD-COST         HISTORY
             │                  │                  │
       Rules / Checks      Rates / Inputs      Revisions
       Measurements        Cost Model          Snapshots
       Findings            Completeness        Audit Trail
```

The goal is not to replace CAD/CAM software.

CodeAsMetal provides a structured engineering layer between product definition,
manufacturing planning, engineering review, and decision support.

---

## Current Status

CodeAsMetal is currently a **V1 engineering preview**.

The Windows Debug/x64 build has been exercised using Visual Studio 2026,
MSVC, CMake, Ninja, Qt 6, OpenCASCADE, GoogleTest, and the project's dedicated
vcpkg environment.

The current automated Windows test suite contains **24 tests**, including core
behavior and CAD/document integration tests.

V1 is not yet considered a stable `1.0` release. See:

```text
docs/acceptance-and-scope.md
```

before treating the preview as production-ready.

---

# What V1 Does

| Area | Current capability |
|---|---|
| CAD | STEP/STP and STL import, SHA-256 verification, STEP units in mm, explicit STL scale |
| Viewer | OCCT AIS visualization, orbit, pan, zoom, fit, standard views, face/edge/solid selection |
| Geometry | Unique topology, bounding box, area, volume and centroid for a valid single solid |
| Features | B-Rep surface classification and cylindrical cavity candidates with engineer confirmation |
| DFM | Depth/diameter, thickness and minimum-radius screening with revision-controlled rule values |
| Process Planning | Preliminary manufacturing plan with editable and reorderable operations |
| Resources | Machine, tool, setup, material and inspection references |
| Costing | Material, setup, machine, labor, tooling, inspection, logistics and overhead |
| Engineering Review | Geometry-linked issues, severity, owner, decision, status and recommendation |
| History | Revision hashes, aggregate comparison and immutable estimate snapshots |
| Persistence | Atomic local project files, recovery, SQL Server/QODBC persistence and rowversion concurrency |
| Outputs | HTML/PDF engineering reports, STEP/STL sample parts and rotating local logs |
| Verification | Core tests, CAD/document tests, CI support and Windows packaging infrastructure |

---

# Engineering Philosophy

CodeAsMetal deliberately separates **geometric recognition** from
**engineering interpretation**.

A cylindrical surface can be recognized geometrically, for example, but that
does not automatically establish its manufacturing intent.

Therefore:

> Recognition does not imply engineering classification.

The system can propose cylindrical cavity candidates, while the engineer
confirms their functional classification and relevant measurements.

Through holes, blind holes, pockets, slots, chamfers, fillets and thin-wall
conditions can be represented with geometry references and measurements, but
V1 does not claim to provide a universal topology-based feature recognition
engine.

This distinction is intentional.

---

# What V1 Does Not Claim

CodeAsMetal V1 does **not** currently provide:

- automatic manufacturing planning based on tool accessibility;
- complete enterprise machine/tool/material catalogs;
- physics-based machining-time prediction;
- automatic geometric identity matching between revisions;
- automatic PMI/GD&T extraction;
- universal machining-feature recognition;
- automatic material, finish or tolerance extraction from arbitrary CAD;
- a generated and signed production Windows installer.

Manufacturing times are engineering inputs.

CodeAsMetal calculates the downstream cost consequences of those inputs rather
than pretending to predict machining time without sufficient process data.

---

# Repository Structure

The repository follows a layered C++ architecture.

```text
CodeAsMetal/
│
├── include/
│   └── cam/
│       └── ...
│
├── src/
│   ├── core/
│   ├── cad/
│   ├── application/
│   ├── infrastructure/
│   └── ui/
│
├── tests/
│
├── data/
│   └── 001_schema.sql
│
├── docs/
│   ├── architecture/
│   ├── testing/
│   ├── acceptance-and-scope.md
│   └── learning-path.md
│
├── legacy/
│   └── Win32Template/
│
├── scripts/
│   ├── prepare-dependencies.ps1
│   └── build.cmd
│
├── CMakeLists.txt
├── CMakePresets.json
├── CodeAsMetal.slnx
└── README.md
```

The exact directory contents may evolve as V1 is refined, but CMake remains the
authoritative build definition.

---

# Technology Stack

### Core

- C++23
- Standard Library
- CMake
- Ninja

### Desktop

- Qt 6 Core
- Qt 6 GUI
- Qt 6 Widgets
- Qt 6 SQL
- Qt 6 PrintSupport

### CAD

- OpenCASCADE Technology (OCCT)
- STEP/STP
- STL
- B-Rep topology and geometry
- AIS visualization

### Persistence

- Local project documents
- SQL Server
- Microsoft ODBC Driver
- Qt QODBC
- SQL Server `rowversion`

### Verification

- GoogleTest
- CTest
- Windows/MSVC tests
- CI infrastructure

### Dependency Management

- vcpkg
- pinned dependency registry
- generated dependency lock information

---

# Requirements

The primary development environment is:

- Windows x64
- Visual Studio 2026
- Desktop development with C++
- MSVC v145
- Windows SDK
- C++ CMake tools for Windows
- Git
- PowerShell
- Internet access for initial dependency acquisition

Qt VS Tools is **not required**.

---

# Getting Started

Clone the repository:

```powershell
git clone https://github.com/Oscarvdo/CodeAsMetal.git
cd CodeAsMetal
```

Do not reuse `.vs`, build output, or generated dependency files from an older
CodeAsMetal checkout.

## 1. Prepare dependencies

Open **Developer PowerShell for VS 2026** and run:

```powershell
.\scripts\prepare-dependencies.ps1
```

The dependency preparation process uses the project's controlled vcpkg
environment and installs the required Qt, OpenCASCADE and GoogleTest
dependencies.

It also records resolved dependency information in:

```text
dependencies.lock.json
```

The first dependency build can take considerable time and disk space because
large C++ dependencies may need to be compiled locally.

If PowerShell execution is restricted by organizational policy, use the
procedure authorized for your machine rather than changing global execution
policy.

After dependency preparation, restart Visual Studio so the environment is
reloaded.

---

# Building with Visual Studio

Open:

```text
CodeAsMetal.slnx
```

Select:

```text
Configuration: Debug
Platform:      x64
```

Set `CodeAsMetal` as the startup project.

Then:

```text
Build → Build Solution
```

The Visual Studio project delegates the authoritative build to CMake.

The build pipeline performs:

```text
Configure
    ↓
Compile
    ↓
Automated Tests
    ↓
Qt Runtime Deployment
```

After a successful build:

```text
F5
```

launches the desktop application.

---

# Building from Developer PowerShell

The same build can be executed without the Visual Studio UI:

```powershell
.\scripts\build.cmd Debug
```

For Release:

```powershell
.\scripts\build.cmd Release
```

The build bridge preserves the project's dedicated `VCPKG_ROOT`, initializes
the Visual Studio x64 compiler environment, selects Visual Studio's CMake and
Ninja, executes the selected CMake preset, runs the tests, and prepares the Qt
runtime required by the desktop executable.

---

# Native CMake Workflow

Developers who prefer Visual Studio's native CMake integration can open the
repository directory containing:

```text
CMakeLists.txt
```

and select:

```text
windows-debug
```

from the available presets.

The solution/project files exist primarily as a convenient Visual Studio entry
point; CMake remains authoritative.

---

# First Engineering Workflow

After launching CodeAsMetal:

### 1. Create a project

Use:

```text
File → New project
```

In V1, a project represents one part with engineering revisions.

### 2. Generate sample geometry

Use:

```text
File → Generate sample CAD…
```

The generated sample geometry provides controlled CAD inputs for exercising the
workflow.

### 3. Import a CAD revision

Use:

```text
File → Import CAD as revision
```

Import a STEP file and assign a revision name such as:

```text
Rev A
```

### 4. Inspect product geometry

Open:

```text
Product / geometry
```

For the supplied `block.step` reference model, the expected volume is:

```text
120000 mm³
```

### 5. Review features

Select geometry in the viewer and inspect its corresponding feature information.

Feature recognition results should be treated as engineering candidates until
confirmed.

Do not confirm a manufacturing feature without reviewing the actual geometry
and available product definition.

### 6. Configure DFM screening

Use:

```text
Engineering → DFM rules for current revision
```

The supplied thresholds are engineering screening examples.

They are **not ISO, ASME or company manufacturing standards**.

Changing an engineering rule threshold requires a new rule version.

### 7. Create a preliminary process plan

Use:

```text
Engineering → Suggest preliminary plan
```

The initial plan intentionally permits unknown operation times.

For each operation, the engineer can define information such as:

```text
Machine
Tool
Setup
Associated features
Cycle time / part
Setup time / batch
Engineering rationale
```

### 8. Enter product and cost inputs

Use:

```text
Engineering → Product and cost inputs
```

Inputs include:

```text
Material
Quantity
Currency
Effective date
Purchased stock mass / part
Tooling / part
Logistics / batch
Overhead
```

An empty value means **unknown**, not zero.

### 9. Add effective-dated rates

Use:

```text
Engineering → Add effective-dated rate
```

Example keys:

```text
material:6061
machine:VMC01
labor:operator
inspection:quality
```

Material rates represent price/kg.

Machine, labor and inspection rates represent price/hour.

CodeAsMetal does not ship with fabricated commercial prices.

### 10. Calculate should-cost

The Should-Cost view distinguishes:

```text
Known subtotal
Missing categories
Input completeness
```

Missing information is not silently interpreted as zero cost.

### 11. Record engineering issues

Use:

```text
Engineering → Add review issue
```

Issues can capture:

```text
Geometry reference
Problem
Owner
Severity
Decision
Status
Comment
Recommendation
```

### 12. Issue an estimate snapshot

An estimate snapshot records the engineering inputs and calculated results used
at the time the estimate was issued.

Partial estimates are permitted, but remain explicitly identified as
incomplete.

---

# CAD and Product Definition Boundaries

CodeAsMetal does not automatically infer all product requirements from CAD.

V1 does not automatically extract:

```text
Material
Finish
Tolerance
GD&T
Manufacturing intent
```

from arbitrary STEP or PDF documents.

PDF documents are referenced using their path and hash rather than embedded
inside the project document.

This preserves provenance without turning the project file into a document
container.

---

# SQL Server

SQL Server is optional.

It is not required for importing CAD or learning the local engineering
workflow.

For shared project persistence:

1. Install or configure SQL Server.
2. Install **Microsoft ODBC Driver 18 for SQL Server x64**.
3. Create an empty database named:

```text
CodeAsMetal
```

4. Use compatibility level 150 or later.
5. Execute:

```text
data/001_schema.sql
```

using an appropriately privileged database account.

6. Have the DBA assign authorized Windows users/groups to the application role
defined by the schema.
7. Connect through:

```text
SQL Server → Connect
```

using integrated authentication and a trusted TLS configuration.

---

# Concurrency Model

Shared SQL Server projects use optimistic concurrency through SQL Server
`rowversion`.

If two users modify the same project version, the second conflicting save is
rejected rather than silently overwriting the first user's work.

The intended recovery workflow is:

```text
Detect conflict
      ↓
Preserve local recovery state
      ↓
Reload server version
      ↓
Review differences
      ↓
Reapply intentional changes
```

V1 does not provide automatic semantic merge.

CAD and PDF files should reside on paths accessible to all participating users,
preferably controlled UNC locations.

The SQL database stores project metadata and engineering snapshots, not the
original CAD files.

Local `.cam.json` files are not intended to act as a multiuser synchronization
mechanism.

---

# Verification

CodeAsMetal has multiple verification layers.

The core was previously exercised independently under GCC 13.3 using C++23,
warnings-as-errors, AddressSanitizer and UndefinedBehaviorSanitizer.

The current Windows integration has additionally been exercised using:

```text
Visual Studio 2026
MSVC x64
CMake
Ninja
Qt 6
OpenCASCADE
GoogleTest / CTest
```

The current Windows test suite reports:

```text
24/24 tests passed
0 failed
```

This includes:

```text
Core behavior tests
CAD/document integration tests
```

Additional validation remains appropriate before declaring a stable production
`1.0` release, particularly around deployment, GPU/viewer behavior, real SQL
Server environments, exported reports, packaging and installer behavior.

---

# Dependency Reproducibility

CodeAsMetal does not rely on whichever copy of Qt, OCCT or vcpkg happens to be
available globally on a development machine.

The dependency preparation workflow uses a dedicated vcpkg installation and
records the resolved dependency state.

The current V1 dependency baseline uses:

```text
vcpkg registry: 2025.12.12
Qt:             6.x
OpenCASCADE:    7.x
GoogleTest
```

See:

```text
dependencies.lock.json
```

for the exact resolved commit and package versions for a prepared environment.

---

# Original Win32 Template

The original educational Win32 implementation has been retained under:

```text
legacy/Win32Template/
```

The V1 desktop application itself starts through `QApplication`, with Qt owning
the application message loop and widget lifecycle.

The Visual Studio `.vcxproj` is intentionally a Makefile-style bridge to CMake
instead of maintaining a second independent dependency and compiler
configuration.

Only x64 is supported by the current V1 Windows configuration.

Do not mix Win32 and x64 libraries or DLLs.

---

# Engineering Documentation Standard

CodeAsMetal source code is documented as engineering software rather than as a
tutorial.

Documentation and comments should explain:

- intent;
- component responsibility;
- public API contracts;
- non-obvious engineering decisions;
- assumptions;
- units;
- algorithms and formulas;
- provenance of engineering values where relevant;
- important constraints and failure modes.

Comments should **not** merely restate C++ syntax.

This documentation standard applies to new CodeAsMetal code as the project
evolves.

---

# Learning the Codebase

A recommended study sequence is:

```text
Domain
  ↓
Core engineering logic
  ↓
CAD representation
  ↓
Project/document model
  ↓
Application services
  ↓
Qt UI
  ↓
SQL persistence
```

Start with:

```text
include/cam/Domain.h
src/core/Domain.cpp
```

Then read:

```text
docs/learning-path.md
docs/architecture/overview.md
```

The DFM and costing logic is intentionally separable from Qt, SQL Server and
OpenCASCADE so the engineering domain can be studied and tested independently.

---

# Roadmap

V1 establishes the vertical engineering workflow:

```text
CAD
 ↓
Geometry
 ↓
Feature Candidates
 ↓
Engineering Confirmation
 ↓
DFM
 ↓
Process Plan
 ↓
Resources
 ↓
Should-Cost
 ↓
Engineering Review
 ↓
Revision / Estimate History
```

Future work can deepen individual layers without collapsing them into a single
automatic "CAD-to-answer" pipeline.

Major areas beyond the current V1 scope include:

```text
Normalized enterprise catalogs
Tool-accessibility analysis
Deeper manufacturing feature recognition
Process knowledge models
More rigorous machining-time models
Cross-revision geometric identity
PMI/GD&T integration
Engineering rule libraries
Expanded validation datasets
Production packaging
```

---

# Design Principle

CodeAsMetal is built around a simple idea:

> **Engineering software should preserve the reasoning behind a decision, not
> only its final number.**

Geometry is evidence.

Manufacturing rules provide context.

Engineers make decisions.

CodeAsMetal keeps those decisions connected to the data, assumptions,
revisions, and cost consequences that produced them.

---

## License

See `LICENSE` for the repository's licensing terms.