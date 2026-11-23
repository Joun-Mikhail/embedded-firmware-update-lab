# Limitations

This repository is an educational **simulation**. Please read these points
before drawing conclusions from it.

| Area | What this project does **not** do |
|------|------------------------------------|
| Hardware | No physical microcontroller. Everything runs as a normal host process. |
| Flash memory | No flash driver, erase/program cycles, page alignment, or wear behaviour. The "installed slot" is a `std::vector`. |
| Bootloader | No bootloader is executed; nothing ever jumps to the received image. |
| Secure boot | Not implemented. There is no root of trust and no chain of trust. |
| Authentication | No signatures, MACs or keys. **CRC-32 detects accidental corruption only**; an attacker can trivially forge a matching CRC. The project must not be described as a secure or authenticated updater. |
| Power-loss safety | The atomic commit is an in-memory swap. There is no guarantee about behaviour if power fails mid-update, and no persistent state survives a restart. |
| Integrity | CRC-only, both per packet and for the whole image. CRC-32 misses some multi-bit error patterns (with probability ~2⁻³² for random corruption). |
| Transport | No UART/USB/CAN/network layer, retransmission, timeouts or flow control. Packet boundaries are assumed to be delivered by the transport. |
| Concurrency / RTOS | Single-threaded; no RTOS, interrupts or concurrent access to the receiver. |
| Provisioning | No production provisioning, device identity, key management or manufacturing flow. |
| Resource limits | The 64 KiB image limit and 256-byte payload limit are simulator choices, not statements about any specific MCU. |
| Testing scope | Tests and sanitizers check the executed paths only. The bounded robustness test (1 200 seeded inputs) is not a coverage-guided fuzzing campaign. Passing sanitizers does not prove the absence of memory errors. |
| Platforms | Verified status per platform is listed in the README; anything not run in CI or locally is unverified. |
| Certification | This is not a security assessment or certification of any kind. |
