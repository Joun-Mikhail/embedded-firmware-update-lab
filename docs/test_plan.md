# Test Plan and Results

* **Harness:** self-contained (`tests/testkit.hpp`), one executable `fwupdate_tests`.
* **Registration:** CMake scans `tests/test_*.cpp` for `FW_TEST(...)` and adds each
  as its own CTest test, plus `DEMO_end_to_end` (runs the demo, which exits
  non-zero on any unexpected behaviour). **38 CTest tests** in total.
* **Independence:** every test builds its own receiver and packet stream; there
  is no shared mutable state between tests and CTest runs each test in its own
  process.
* **Negative tests assert the exact `Status`.** A test that expects
  `INVALID_SEQUENCE` fails if the packet is rejected for any other reason. Where
  a mutation would otherwise be caught by the packet CRC first, the test calls
  `fault::reseal_crc()` so the intended check is the one that fires.
* **Atomicity:** `CHECK_INSTALLED_IS_V1(rx)` compares both the installed version
  and every installed byte against the original image.
* **Determinism:** images come from `generate_image()` (xorshift32 with fixed
  seeds); the robustness test uses `std::mt19937` with seed `20261123`, whose
  output sequence is fixed by the C++ standard.

Run a single scenario: `ctest --test-dir build -R FW_006 --output-on-failure`
or `./build/tests/fwupdate_tests FW_006_image_corruption_with_valid_packet_crc`.

## A. Packet encoding and decoding

| ID | Test | Requirement | Expected result |
|----|------|-------------|-----------------|
| PKT-001 | `PKT_001_valid_round_trip` | Encode/decode are inverses; exact byte layout | All fields equal for START/DATA/FINISH/ABORT, header bytes match the spec, CRC big-endian |
| PKT-002 | `PKT_002_crc32_known_answer` | CRC-32/ISO-HDLC | `crc32("123456789") == 0xCBF43926` |
| — | `CRC_empty_input`, `CRC_binary_vectors`, `CRC_incremental_equals_one_shot` | CRC edge cases | Empty → `0`; vectors match Python `zlib.crc32`; streaming = one-shot |
| PKT-003 | `PKT_003_invalid_magic` | Magic check | `INVALID_MAGIC`, also with a valid CRC |
| PKT-004 | `PKT_004_unsupported_version` | Version check | `UNSUPPORTED_VERSION` for 0, 2, 255 |
| PKT-005 | `PKT_005_truncated_packet` | Truncation at every length 0..23 | `INVALID_LENGTH` for each |
| PKT-006 | `PKT_006_incorrect_advertised_length` | Length field consistency, no trailing bytes | `INVALID_LENGTH` |
| PKT-007 | `PKT_007_invalid_packet_type` | Unknown type values | `INVALID_PACKET_TYPE` (decoder and encoder) |
| PKT-008 | `PKT_008_oversized_payload` | Max payload 256 | 257 → `PAYLOAD_TOO_LARGE`; 256 → OK |
| PKT-009 | `PKT_009_corrupted_payload_crc` | Packet CRC | All 48 single-bit payload flips, a damaged CRC field and an unsealed header change → `CRC_MISMATCH` |
| PKT-010 | `PKT_010_reserved_field_nonzero` | Reserved = 0 | `INVALID_RESERVED` |
| — | `PKT_011_empty_and_minimal_buffers` | Empty packets, per-type payload rules | Empty buffer → `INVALID_LENGTH`; empty DATA, short START, FINISH/ABORT with payload → `INVALID_PAYLOAD` |
| — | `PKT_012_overflow_sensitive_lengths` | Length 0xFFFF | `PAYLOAD_TOO_LARGE`, no over-read (also run under ASan) |

## B. Firmware update behaviour

| ID | Test | Requirement | Expected result |
|----|------|-------------|-----------------|
| FW-001 | `FW_001_valid_update_v1_to_v2` | Valid update | OK, VERIFIED, v2 bytes installed |
| FW-002 | `FW_002_multiple_data_packets` | 215 DATA chunks of 7 B | Installed slot unchanged until FINISH, then v2 |
| FW-003 | `FW_003_out_of_order_data` | Ordering | `INVALID_SEQUENCE` at the swapped packet |
| FW-004 | `FW_004_duplicate_data` | Duplicates | `INVALID_SEQUENCE` at the copy |
| FW-005 | `FW_005_missing_data` | Missing packet | Gap → `INVALID_SEQUENCE`; last DATA dropped and FINISH renumbered → `IMAGE_INCOMPLETE` |
| FW-006 | `FW_006_image_corruption_with_valid_packet_crc` | Image CRC is not redundant | Every packet decodes OK; FINISH → `IMAGE_CRC_MISMATCH` |
| FW-007 | `FW_007_version_rollback` | Older version | `VERSION_ROLLBACK` at START |
| FW-008 | `FW_008_equal_version_rejected` | Strictly increasing versions | Equal version → `VERSION_ROLLBACK` |
| FW-009 | `FW_009_abort_active_update`, `FW_009b_abort_rules` | Abort | `UPDATE_ABORTED`, IDLE, v1 kept; ABORT in IDLE → `INVALID_STATE`; stale ABORT → `INVALID_SEQUENCE` |
| FW-010 | `FW_010_failed_update_atomicity_matrix` | Atomicity | 13 failure classes, each with the exact status and v1 bytes + version intact |
| FW-011 | `FW_011_recovery_after_failure`, `FW_011b_repeated_failures_then_recovery` | Recovery | FAILED is sticky until `reset()`; 5 failed attempts then success |
| FW-012 | `FW_012_image_size_boundaries` | Size limits | 65 536 B installs; 65 537 B and 0xFFFFFFFF → `IMAGE_TOO_LARGE`; 0 B → `INVALID_LENGTH` |

## C. Additional checks

| Test | Behaviour |
|------|-----------|
| `FW_013_data_or_finish_before_start` | DATA/FINISH in IDLE → `INVALID_STATE` |
| `FW_014_packets_after_completion` | Late DATA / repeated FINISH after VERIFIED → `INVALID_STATE`; replay after reset → `VERSION_ROLLBACK` |
| `FW_015_start_while_receiving` | Second START → `INVALID_STATE` |
| `FW_016_start_with_nonzero_sequence` | START seq ≠ 0 → `INVALID_SEQUENCE` |
| `FW_017_handle_rejects_malformed_packet_objects` | `handle()` applies the decoder's payload and type rules; an undefined type value fails closed from IDLE and mid-session |
| `FW_018_data_exceeds_declared_length` | Extra DATA → `IMAGE_TOO_LARGE` |
| `FLT_001_mutators_respect_bounds` | Fault helpers never write out of range |
| `ROB_001_deterministic_malformed_inputs` | 1 200 seeded inputs (random bytes, mutated packets with and without re-sealed CRC, random cut/extend): every accepted packet re-encodes to the identical bytes, receiver never installs anything |
| `DEMO_end_to_end` | Demo exit code 0 |

## Fault-injection matrix

Each row: valid behaviour → mutation → expected rejection → resulting state →
installed firmware. All rows are executed by `FW_010` (unless noted).

| Mutation (helper) | Without mutation | Expected status | State after | Installed |
|-------------------|------------------|-----------------|-------------|-----------|
| Flip payload bit (`flip_payload_bit`) | packet accepted | `CRC_MISMATCH` | FAILED | v1 unchanged |
| Corrupt packet CRC (`corrupt_crc`) | accepted | `CRC_MISMATCH` | FAILED | v1 unchanged |
| Truncate packet (`truncate`) | accepted | `INVALID_LENGTH` | FAILED | v1 unchanged |
| Change length field (`set_payload_length_field`) | accepted | `INVALID_LENGTH` | FAILED | v1 unchanged |
| Replace magic (`set_magic`) | accepted | `INVALID_MAGIC` | FAILED | v1 unchanged |
| Replace version + reseal (`set_version`) | accepted | `UNSUPPORTED_VERSION` | FAILED | v1 unchanged |
| Change type DATA→START/FINISH + reseal (`set_type`) | accepted | `INVALID_PAYLOAD` | FAILED | v1 unchanged |
| Replace sequence + reseal (`set_sequence`) | accepted | `INVALID_SEQUENCE` | FAILED | v1 unchanged |
| Drop packet (`drop_packet`) — FW-005 | update completes | `INVALID_SEQUENCE` / `IMAGE_INCOMPLETE` | FAILED | v1 unchanged |
| Duplicate packet (`duplicate_packet`) — FW-004 | update completes | `INVALID_SEQUENCE` | FAILED | v1 unchanged |
| Reorder DATA (`swap_packets`) — FW-003 | update completes | `INVALID_SEQUENCE` | FAILED | v1 unchanged |
| Corrupt image byte + reseal packet CRC | update completes | `IMAGE_CRC_MISMATCH` at FINISH | FAILED | v1 unchanged |
| Truncated stream (FINISH never sent) | update completes | (no rejection) | RECEIVING | v1 unchanged |
| Older target version | update completes | `VERSION_ROLLBACK` | FAILED | v1 unchanged |
| ABORT after two DATA | update completes | `UPDATE_ABORTED` | IDLE | v1 unchanged |

## Actual verification evidence

| Configuration | Result | Evidence |
|---------------|--------|----------|
| GCC 13.3, Debug, `-Werror` (local, Ubuntu 24.04) | 38/38 passed, 0 warnings | [`assets/evidence/ctest_gcc.txt`](../assets/evidence/ctest_gcc.txt) |
| Clang 18.1, Debug, `-Werror` (local) | 38/38 passed, 0 warnings | local run, see delivery notes |
| GCC 13.3 + ASan + UBSan (local) | 38/38 passed, no sanitizer reports | [`assets/evidence/ctest_sanitizers_gcc.txt`](../assets/evidence/ctest_sanitizers_gcc.txt) |
| Clang 18.1 + ASan + UBSan | 38/38 passed in CI (the local Clang install lacks the ASan runtime, so this ran in CI only) | GitHub Actions `sanitizers` job |
| GCC 13.3 / Clang 18.1 × Debug/Release, `-Werror` | 38/38 passed in each CI job, demo exit 0 | GitHub Actions `linux` jobs (JUnit XML uploaded as artifacts) |
| MSVC 19.51 (VS Build Tools 2026), Release, `/W4 /WX` | 38/38 passed, demo exit 0, 0 warnings | Author's own Windows 11 PC, 2026-11-23 ([screen captures](../README.md#see-it-run)) |
| MSVC 19.44 (Visual Studio 2022), Release, `/W4 /WX` | 38/38 passed in CI, demo exit 0, 0 warnings | GitHub Actions `windows` job |
| Demo | exit 0 | [`assets/evidence/demo_output.txt`](../assets/evidence/demo_output.txt) |
| Controlled regression experiment | defects detected, then reverted | [regression_experiment.md](regression_experiment.md) |
