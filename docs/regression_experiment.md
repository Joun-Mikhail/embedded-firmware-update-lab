# Controlled Regression Experiment

**Purpose:** show that the suite detects real defects, not only that it passes.
Two deliberate defects were introduced one at a time into
`src/receiver.cpp`, the suite was run, and the file was restored with
`git checkout -- src/receiver.cpp`. These are synthetic defects in this
simulator, not vulnerabilities found in any real product.

Environment: Ubuntu 24.04, GCC 13.3, CMake 3.28, Debug build, 2026-11-23.
Output below is copied from the recorded runs (only the absolute checkout path
was shortened).

## Mutation A — complete-image CRC check disabled

```diff
-    if (crc32(staging_) != session_.image_crc) {
+    if (false && crc32(staging_) != session_.image_crc) { // MUTATION A
         return reject(Status::ImageCrcMismatch);
```

Result: **3 of 38 tests failed** (`ctest` exit code 8).

```text
34/38 Test #34: FW_006_image_corruption_with_valid_packet_crc ....***Failed    0.00 sec
[ RUN  ] FW_006_image_corruption_with_valid_packet_crc - Corrupted image bytes with re-sealed packet CRCs are caught only by the image CRC
    FAILED at tests/test_faults.cpp:77
      CHECK_EQ(r.status, Status::ImageCrcMismatch)
      actual:   OK
      expected: IMAGE_CRC_MISMATCH
    ...
    FAILED at tests/test_faults.cpp:79
      CHECK_EQ(rx.state(), UpdateState::Failed)
      actual:   VERIFIED
      expected: FAILED
    FAILED at tests/test_faults.cpp:80
      CHECK_EQ((rx).installed_version(), helpers::kV1)
      actual:   2
      expected: 1

The following tests FAILED:
	  1 - DEMO_end_to_end (Failed)
	 34 - FW_006_image_corruption_with_valid_packet_crc (Failed)
	 35 - FW_010_failed_update_atomicity_matrix (Failed)
```

`FW_010` failed on exactly one row, `(scenario: image bytes corrupted, CRC
resealed)`. The demo also refused to report success:

```text
[SAFE  ] Installed firmware remains v3 (bytes CHANGED), state VERIFIED
[ERROR ] unexpected behaviour: installed firmware changed after a failed update
...
RESULT: 3 expectation(s) FAILED
```

This is the key result for the design: with the image CRC disabled, a
corrupted image whose packets all carry valid CRCs **is installed**. The
per-packet CRC alone does not protect the image.

## Mutation B — equal version accepted (rollback policy weakened)

```diff
-    if (info->target_version <= installed_version_) {
+    if (info->target_version < installed_version_) { // MUTATION B
         return reject(Status::VersionRollback);
```

Result: **2 of 38 tests failed**.

```text
20/38 Test #20: FW_008_equal_version_rejected ....................***Failed    0.00 sec
    FAILED at tests/test_receiver.cpp:66
      CHECK_EQ(feed(rx, *stream).status, Status::VersionRollback)
      actual:   OK
      expected: VERSION_ROLLBACK
    FAILED at tests/test_receiver.cpp:67
      CHECK((rx).installed_image() == helpers::v1_image())
27/38 Test #27: FW_014_packets_after_completion ..................***Failed    0.00 sec
    FAILED at tests/test_receiver.cpp:182
      CHECK_EQ(feed(rx, stream).status, Status::VersionRollback)
      actual:   OK
      expected: VERSION_ROLLBACK
```

`FW_007_version_rollback` (strictly *older* version) kept passing, which is
correct: the mutation only changes the equal-version boundary, and the
boundary test is the one that caught it.

## Restoration

After each mutation the file was restored from Git and the change confirmed
gone (`grep -c MUTATION src/receiver.cpp` → `0`). The full suite was then
rebuilt and rerun:

```text
100% tests passed, 0 tests failed out of 38      # GCC, Debug
100% tests passed, 0 tests failed out of 38      # GCC, ASan + UBSan
```

No deliberate defect exists in the published source.
