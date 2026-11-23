<div align="center">

# Embedded Firmware Update & Fault Injection Lab

### Making sure a device can never be "bricked" by a bad software update

**C++17 · binary protocol · CRC-32 integrity · update state machine · fault injection · automated testing**

[![CI](https://github.com/Joun-Mikhail/embedded-firmware-update-lab/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Joun-Mikhail/embedded-firmware-update-lab/actions/workflows/ci.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-1F6FEB?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/build-CMake%20%2B%20CTest-0E7C86?logo=cmake&logoColor=white)
![Tests](https://img.shields.io/badge/tests-38%20passing-2E8B57)
![Platforms](https://img.shields.io/badge/verified%20on-Linux%20%C2%B7%20Windows-1F6FEB)
![Sanitizers](https://img.shields.io/badge/ASan%20%2B%20UBSan-clean-2E8B57)
![License](https://img.shields.io/badge/license-MIT-5A6B85)

</div>

---

## In plain words

Every smart device — a car's control unit, a pacemaker, a router, an IoT sensor — receives
**firmware updates**. If an update arrives damaged, out of order or incomplete and the device
installs it anyway, the device can stop working completely ("bricked").

This project is a **software model of a device receiving an update**. It checks every piece
of the update as it arrives, checks the finished file as a whole, and only then swaps it in.
Then it **attacks itself on purpose** — flipping single bits, dropping or repeating pieces,
lying about sizes — and proves, with automated tests, that the old firmware always survives.

> 🟢 **Good update →** checked piece by piece, verified as a whole, installed in one step.
> 🔴 **Damaged update →** rejected, and the device keeps running its previous version, untouched.

## At a glance

| | |
|---|---|
| 🎯 **Goal** | Prove that a failed update can never change the installed firmware |
| 🧱 **Built with** | C++17, CMake, CTest, GitHub Actions — standard library only, no hardware needed |
| 🧪 **Automated tests** | **38**, all passing, on Linux (GCC, Clang) and Windows (MSVC) |
| 💥 **Fault types injected** | **15** — bit flips, corrupted checksums, cut-off packets, missing, duplicated and reordered packets, fake versions, and more |
| 🎲 **Random robustness inputs** | **1 200** malformed packets per run, reproducible from a fixed seed |
| 🛡️ **Memory-safety checks** | AddressSanitizer + UndefinedBehaviorSanitizer report **zero** issues |
| 🔁 **Tests that catch real bugs** | Two bugs were planted on purpose — the suite caught both ([details](docs/regression_experiment.md)) |
| ⚙️ **Continuous integration** | **7** automated jobs on every change: 2 compilers × 2 build types, 2 sanitizer builds, Windows |

## See it run

### On a Windows PC

Real screenshots from the author's own Windows 11 PC (23 Nov 2026): Visual Studio Build Tools 2026,
MSVC 19.51, Windows SDK 10.0.26100, warnings treated as errors.

<p align="center">
  <img src="assets/screenshots/windows_demo.jpg" width="760"
       alt="Windows Terminal on the author's PC running fwupdate_demo.exe: update to v2 passes and is installed; a v3 update with one flipped bit passes every packet check but fails the whole-image check, so v2 stays installed; after a reset a clean v3 is accepted; result: update recovery and atomicity verified">
  <br><sub><b>① The demo</b> — a corrupted update is caught and the device keeps its working firmware.</sub>
</p>

<p align="center">
  <img src="assets/screenshots/windows_tests.jpg" width="760"
       alt="Windows Terminal on the author's PC running ctest: tests 18 to 38 shown as Passed, ending with 100% tests passed, 0 tests failed out of 38">
  <br><sub><b>② The test suite</b> — <code>ctest</code> reports <b>100% tests passed, 0 failed out of 38</b>.</sub>
</p>

<details>
<summary>③ The build (CMake + MSVC, warnings as errors)</summary>
<p align="center">
  <img src="assets/screenshots/windows_build.jpg" width="760"
       alt="Windows Terminal on the author's PC: cmake 4.3.1 configures the project for Visual Studio 18 2026 with Windows SDK 10.0.26100 and MSVC 19.51, registers 37 test cases, then builds the core and fault libraries, the demo and the test executable">
  <br><sub>CMake 4.3.1 configures for Visual Studio 2026 / MSVC 19.51 and builds the libraries, demo and tests.</sub>
</p>
</details>

### On Linux and in CI

<p align="center">
  <img src="assets/screenshots/demo.png" width="720"
       alt="Terminal output of fwupdate_demo on Linux: v1 to v2 update passes and is committed; a v3 update with an injected bit flip passes every packet CRC but fails the image CRC at FINISH; firmware remains v2; after reset a clean v3 is accepted">
  <br><sub><b>Demo on Linux</b> (GCC 13.3, Ubuntu 24.04) — rendered from the recorded terminal output in <code>assets/evidence/</code>.</sub>
</p>

<p align="center">
  <img src="assets/screenshots/ci_actions.jpg" width="620"
       alt="GitHub Actions run summary page showing all seven CI jobs successful: Linux gcc Debug and Release, Linux clang Debug and Release, ASan plus UBSan for gcc and clang, and Windows MSVC">
  <br><sub><b>GitHub Actions</b> — all 7 jobs green (browser capture of run <a href="https://github.com/Joun-Mikhail/embedded-firmware-update-lab/actions/runs/45277523799">#4</a>, commit <code>a43f70d</code>).</sub>
</p>

<details>
<summary>More evidence: full Linux test and sanitizer runs</summary>
<p align="center">
  <img src="assets/screenshots/ctest.png" width="620" alt="CTest output listing 38 tests, all passed, GCC 13.3 build with warnings as errors">
  <img src="assets/screenshots/ctest_sanitizers.png" width="620" alt="CTest output of the AddressSanitizer and UndefinedBehaviorSanitizer build, 38 of 38 tests passed">
  <br><sub>Rendered from the recorded logs in <code>assets/evidence/</code>.</sub>
</p>
</details>

## What this project demonstrates

| Skill | Where you can see it |
|-------|----------------------|
| **Embedded / low-level C++** — fixed-width integers, byte-exact parsing, no undefined behaviour | [`src/packet.cpp`](src/packet.cpp), [`src/byte_io.hpp`](src/byte_io.hpp) |
| **Protocol design & documentation** — a spec another engineer could implement from | [Protocol specification](docs/protocol_specification.md) |
| **Firmware update logic** — state machine, version policy, all-or-nothing install | [`src/receiver.cpp`](src/receiver.cpp), [State machine](docs/state_machine.md) |
| **Data integrity** — CRC-32 per packet and over the whole image | [`src/crc32.cpp`](src/crc32.cpp) |
| **Test engineering** — negative testing, boundary cases, fault injection, reproducibility | [Test plan](docs/test_plan.md), [`tests/`](tests) |
| **Verification mindset** — sanitizers, warnings-as-errors, proving the tests can fail | [Regression experiment](docs/regression_experiment.md) |
| **CI/CD** — cross-platform GitHub Actions pipeline | [`.github/workflows/ci.yml`](.github/workflows/ci.yml) |
| **Honest scoping** — what the project does *not* prove | [Limitations](docs/limitations.md) |

## How it works

```mermaid
flowchart LR
    A["📦 New firmware"] --> B["✂️ Split into<br/>numbered packets"]
    B --> C{"🔍 Each packet<br/>intact & in order?"}
    C -- no --> F["🔴 Reject update<br/>old firmware kept"]
    C -- yes --> D["🗂️ Collect in a<br/>staging area"]
    D --> E{"🔍 Whole image<br/>checksum correct?"}
    E -- no --> F
    E -- yes --> G["🟢 Install in one step<br/>new version active"]

    classDef ok fill:#2E8B57,stroke:#0B1628,color:#ffffff
    classDef bad fill:#C0392B,stroke:#0B1628,color:#ffffff
    classDef step fill:#1F6FEB,stroke:#0B1628,color:#ffffff
    classDef check fill:#0E7C86,stroke:#0B1628,color:#ffffff
    class A,B,D step
    class C,E check
    class F bad
    class G ok
```

---

# Technical deep dive

*The sections below are for engineers and reviewers.*

## Engineering capabilities

| | |
|---|---|
| **Binary protocol** | Documented big-endian wire format (`FWUP` v1), 14-byte header, 0–256-byte payload, CRC-32 trailer. Strict parsing: lengths are checked before any read, no packed-struct casts, trailing bytes rejected. |
| **Two-level integrity** | CRC-32/ISO-HDLC per packet **and** over the complete image, verified against the `0xCBF43926` known-answer vector. |
| **Update state machine** | `IDLE → RECEIVING → VERIFIED / FAILED`, strict sequence numbers, strictly increasing version policy, explicit `reset()` recovery and ABORT handling. |
| **Atomic commit** | Data lands in a staging buffer; image and version change together via a `noexcept` swap only after every check passes. |
| **Fault injection** | 15 deterministic mutators: bit flips, CRC corruption, truncation, length/magic/version/type/sequence tampering, drop/duplicate/reorder, and image corruption behind *valid* packet CRCs. |
| **Verification** | 38 CTest tests, warning-clean with `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`, ASan + UBSan, a seeded 1 200-case robustness test, and a controlled regression experiment. |

## Architecture

```mermaid
flowchart LR
    subgraph S["Sender"]
        A["Firmware image"] --> B["build_update()<br/>START · DATA… · FINISH"] --> C["encode_packet()"]
    end
    subgraph T["Test infrastructure"]
        F["fault:: mutators"]
    end
    subgraph R["Receiver — fwupdate_core"]
        D["decode_packet()<br/>framing · magic · version · length"] --> E["Packet CRC-32<br/>type · reserved · payload rules"]
        E --> G["FirmwareUpdateReceiver<br/>state · sequence · version policy"]
        G --> H["Staging buffer"]
        H -->|"FINISH: size + image CRC ok<br/>atomic swap"| I["Installed firmware slot"]
    end
    C -->|"bytes"| D
    C -.-> F
    F -.->|"malformed packets / streams"| D

    classDef sender fill:#1F6FEB,stroke:#0B1628,color:#ffffff
    classDef recv fill:#0E7C86,stroke:#0B1628,color:#ffffff
    classDef slot fill:#2E8B57,stroke:#0B1628,color:#ffffff
    classDef fault fill:#B7791F,stroke:#0B1628,color:#ffffff
    class A,B,C sender
    class D,E,G,H recv
    class I slot
    class F fault
```

Component responsibilities and design decisions: [docs/architecture.md](docs/architecture.md).

## Binary protocol

<p align="center">
  <img src="assets/diagrams/packet_layout.svg" width="900"
       alt="Packet layout: magic FWUP at offset 0 (4 bytes), version at 4, type at 5, reserved at 6 (2 bytes), sequence at 8 (4 bytes), payload length at 12 (2 bytes), payload at 14 (0 to 256 bytes), CRC-32 at 14+N (4 bytes) covering bytes 0 to 14+N">
</p>

| Type | Code | Payload | Sequence |
|------|-----:|---------|----------|
| START | `0x01` | target version · image length · image CRC-32 (12 B) | `0` |
| DATA | `0x02` | 1–256 image bytes | `1, 2, 3, …` |
| FINISH | `0x03` | — | last DATA + 1 |
| ABORT | `0x04` | — | next expected |

Full specification, validation order and a byte-exact example:
[docs/protocol_specification.md](docs/protocol_specification.md).

## Update state machine

```mermaid
stateDiagram-v2
    direction LR
    [*] --> IDLE
    IDLE --> RECEIVING: valid START
    RECEIVING --> RECEIVING: DATA (next seq)
    RECEIVING --> VERIFIED: FINISH ✓ size + image CRC → commit
    RECEIVING --> IDLE: ABORT → UPDATE_ABORTED
    IDLE --> FAILED: rejected packet
    RECEIVING --> FAILED: rejected packet
    VERIFIED --> IDLE: reset()
    FAILED --> IDLE: reset()
```

**Invariant:** the installed image and version change together, only at a verified FINISH.
Any rejected packet ends the session in `FAILED` with the previous firmware intact.
Details: [docs/state_machine.md](docs/state_machine.md).

## Valid vs. corrupted update

```mermaid
sequenceDiagram
    participant S as Sender
    participant R as Receiver
    participant I as Installed slot
    Note over I: v1 installed
    S->>R: START v2 · DATA 1..n · FINISH
    R->>R: packet CRCs ✓ · sequence ✓ · size ✓ · image CRC ✓
    R->>I: atomic commit
    Note over I: v2 installed
    S->>R: START v3 · DATA (1 bit flipped, packet CRC re-sealed) · FINISH
    R->>R: packet CRCs ✓ · sequence ✓ · size ✓ · image CRC ✗
    R-->>S: IMAGE_CRC_MISMATCH → FAILED
    Note over I: still v2, bytes unchanged
```

## Fault injection

Every scenario states the valid behaviour, the mutation, the expected rejection, the resulting
state and the preserved firmware. Highlights:

| Mutation | Detected by | Status |
|----------|-------------|--------|
| Single payload bit flip | packet CRC | `CRC_MISMATCH` |
| Packet truncated / length field altered | framing | `INVALID_LENGTH` |
| Magic / version replaced | format check | `INVALID_MAGIC` / `UNSUPPORTED_VERSION` |
| DATA dropped, duplicated or reordered | sequence tracking | `INVALID_SEQUENCE` |
| Last DATA dropped, FINISH renumbered | size check | `IMAGE_INCOMPLETE` |
| **Image byte corrupted, packet CRC re-sealed** | **image CRC only** | `IMAGE_CRC_MISMATCH` |
| Older or equal version | version policy | `VERSION_ROLLBACK` |

```cpp
PacketStream s = *build_update(2, image);
fault::flip_payload_bit(s[4], 17, 3);   // corrupt one image bit
fault::reseal_crc(s[4]);                // ...but keep the packet CRC valid
auto r = feed(receiver, s);             // r.status == Status::ImageCrcMismatch
                                        // receiver.installed_version() == 1
```

Full matrix: [docs/test_plan.md](docs/test_plan.md#fault-injection-matrix).

## Testing and results

| Configuration | Result |
|---------------|--------|
| GCC 13.3, `-Werror` (local) | **38 / 38 passed**, 0 warnings — [log](assets/evidence/ctest_gcc.txt) |
| Clang 18.1, `-Werror` (local) | **38 / 38 passed**, 0 warnings |
| GCC 13.3 + AddressSanitizer + UBSan (local) | **38 / 38 passed**, no reports — [log](assets/evidence/ctest_sanitizers_gcc.txt) |
| MSVC 19.51 (VS Build Tools 2026), Release, `/W4 /WX` (author's own Windows 11 PC) | **38 / 38 passed**, demo OK — [screenshots](#on-a-windows-pc) |
| GitHub Actions: GCC 13.3 & Clang 18.1 × Debug/Release, ASan+UBSan (GCC & Clang), MSVC 19.44 (VS 2022) `/W4 /WX` | **7 / 7 jobs green, 38 / 38 tests in each** — [workflow runs](https://github.com/Joun-Mikhail/embedded-firmware-update-lab/actions/workflows/ci.yml) |
| Controlled regression experiment | disabling the image CRC → 3 tests fail; accepting equal versions → 2 tests fail — [details](docs/regression_experiment.md) |


## CI pipeline

```mermaid
flowchart LR
    P["push / pull request"] --> L["Linux matrix<br/>GCC · Clang × Debug · Release"]
    P --> SA["Sanitizers<br/>ASan + UBSan · GCC · Clang"]
    P --> W["Windows<br/>MSVC 2022 /W4 /WX"]
    L --> L1["configure -Werror"] --> L2["build"] --> L3["ctest --output-junit"] --> L4["run demo"] --> L5["step summary + artifacts"]
    SA --> S1["build -fsanitize=address,undefined"] --> S2["ctest (halt on error)"] --> S3["JUnit artifact"]
    W --> W1["build Release"] --> W2["ctest"] --> W3["run demo"]

    classDef trig fill:#0B1628,stroke:#1F6FEB,color:#ffffff
    classDef job fill:#1F6FEB,stroke:#0B1628,color:#ffffff
    classDef step fill:#0E7C86,stroke:#0B1628,color:#ffffff
    class P trig
    class L,SA,W job
    class L1,L2,L3,L4,L5,S1,S2,S3,W1,W2,W3 step
```

Workflow: [.github/workflows/ci.yml](.github/workflows/ci.yml). Any failing build, test,
sanitizer report or demo expectation fails the job.

## Build and run

Requirements: CMake ≥ 3.16 and a C++17 compiler (GCC, Clang or MSVC). No other dependencies.

**Linux / macOS (Bash)**

```bash
cmake -S . -B build -DFWUPDATE_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/fwupdate_demo --color

# AddressSanitizer + UndefinedBehaviorSanitizer (GCC/Clang)
cmake -S . -B build-asan -DFWUPDATE_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

**Windows (PowerShell, Visual Studio 2022 or 2026 / Build Tools with the C++ workload and a Windows SDK)**

```powershell
# run from a "Developer PowerShell for VS"; CMake picks the newest Visual Studio automatically
cmake -S . -B build -DFWUPDATE_WARNINGS_AS_ERRORS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\fwupdate_demo.exe
```

| CMake option | Default | Effect |
|--------------|---------|--------|
| `FWUPDATE_BUILD_TESTS` | `ON` | Build `fwupdate_tests` and register CTest tests |
| `FWUPDATE_BUILD_DEMO` | `ON` | Build `fwupdate_demo` |
| `FWUPDATE_WARNINGS_AS_ERRORS` | `OFF` | `-Werror` / `/WX` |
| `FWUPDATE_ENABLE_SANITIZERS` | `OFF` | ASan + UBSan on GCC/Clang (ignored with a warning on MSVC) |

## Repository layout

```text
include/fwupdate/   protocol.hpp · crc32.hpp · packet.hpp · receiver.hpp · sender.hpp · fault_injection.hpp
src/                implementation (fwupdate_core + fwupdate_faults)
tests/              self-contained harness and 37 test scenarios
examples/demo.cpp   end-to-end demonstration
docs/               architecture · protocol · state machine · test plan · limitations · regression experiment
assets/             diagrams (SVG + generator), screenshots, recorded evidence logs
```

## Design decisions

- **Explicit result types, no exceptions in the library** — every failure is a specific `Status` (16 values).
- **Byte-wise big-endian I/O** — portable across hosts, no alignment or aliasing assumptions.
- **Length → CRC → semantics** — fields are read only after bounds are proven and interpreted only after the CRC matches.
- **Fail-closed sessions** — one bad packet ends the update; recovery is an explicit `reset()`.
- **Fault injection outside the core library** — production code never links the mutators.
- **Self-contained test harness** — offline builds, each scenario its own CTest test.

More in [docs/architecture.md](docs/architecture.md#design-decisions).

## Limitations

This is a **host-side simulation for learning and validation practice**. It has no real MCU,
flash, bootloader, RTOS or transport layer; the commit is an in-memory swap with no power-loss
guarantee; and **CRC-32 is not authentication** — this is not secure boot or a signed updater.
Sanitizers and tests cover executed paths only. Full list: [docs/limitations.md](docs/limitations.md).

## Documentation

| Document | Contents |
|----------|----------|
| [Architecture](docs/architecture.md) | Components, data flow, design decisions, build targets |
| [Protocol specification](docs/protocol_specification.md) | Wire format, CRC parameters, validation order, receiver rules |
| [State machine](docs/state_machine.md) | States, transitions, lifecycle, atomicity invariant |
| [Test plan](docs/test_plan.md) | Test IDs, expected results, fault-injection matrix, evidence |
| [Regression experiment](docs/regression_experiment.md) | Deliberate defects and the tests that caught them |
| [Limitations](docs/limitations.md) | What this project does not prove |

## License

[MIT](LICENSE) © 2026 Joun Mikhail
