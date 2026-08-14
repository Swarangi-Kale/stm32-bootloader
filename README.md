# STM32G070 Bootloader with UART Firmware Update and Fail-Safe Recovery

A custom, from-scratch bootloader for the STM32G070RB (Cortex-M0+) that receives new application
firmware over UART using a hand-rolled reliable packet protocol, verifies its integrity before
trusting it, and automatically falls back to recovery mode if a newly-flashed application never
proves it's alive — without any vendor bootloader, MCUboot, or RTOS underneath it.

Everything here — the packet protocol, the flash driver, the reset-survivable state machine, the
watchdog-based liveness check — is written directly against the reference manual, not generated
from a HAL wizard.

---

## 1. Overview

The project has three parts that all cooperate over one interface (UART) and one contract (a
firmware header format):

```
┌─────────────────┐         UART, custom packet            ┌──────────────────────┐
│   pc-updater    │  protocol (length+data+CRC8, ACK/RETX) │   mcu-bootloader     │
│  (Node.js / TS, │ ───────────────────────────────────►   │  0x08000000, 32 KB   │
│   host machine) │ ◄───────────────────────────────────   │  (Cortex-M0+)        │
└─────────────────┘                                        └──────────┬───────────┘
                                                                      │ validates CRC32,
                                                                      │ writes boot status,
                                                                      │ jumps
                                                                      ▼
                                                             ┌──────────────────────┐
                                                             │      mcu-app         │
                                                             │  0x08008000, 96 KB   │
                                                             │  confirms itself     │
                                                             │  alive, feeds IWDG   │
                                                             └──────────────────────┘
```

- **`mcu-bootloader`** — lives in the first 32 KB of flash. On every reset it decides, based on a
  status value stored in a backup register (survives reset, not power-loss), whether the current
  application is trustworthy. If yes, it arms a watchdog and jumps to it. If no — or if a host
  tool asks for an update over UART — it stays resident and runs the update protocol.
- **`mcu-app`** — the user application, linked to start at `0x08008000`, immediately after the
  bootloader region. Its only bootloader-related responsibility is to declare itself alive (write
  a confirmation magic value) and keep feeding the independent watchdog (IWDG) once it's running.
- **`pc-updater`** — a Node.js/TypeScript host-side tool that reads a compiled application `.bin`,
  computes and patches in its CRC32 and length into a firmware header, and drives the same packet
  protocol from the PC side to push it over a serial port.
- **`shared/inc`** — the two headers (`firmware_info.h`, `boot_status.h`) that both the bootloader
  and the application (and conceptually, the host updater) agree on. This is the actual "API"
  between all three components — the flash layout, the header format, and the backup-register
  contract all live here.

---

## 2. Design Highlights

### Bootloader State Machine

*[Placeholder: state machine + timeout mechanism diagram]*

This is the UART update-protocol state machine — it governs how the bootloader talks to
`pc-updater` and receives a new image, independent of whether it decides to trust and run that
image afterward (see *Does the Firmware Actually Work?* below).

| State | Action | Next state |
|---|---|---|
| `BL_STATE_SYNC` | Wait for the 4-byte sync sequence from the host | `BL_STATE_WAIT_FOR_UPDATE_REQ` |
| `BL_STATE_WAIT_FOR_UPDATE_REQ` | Wait for an update request packet | `BL_STATE_DEVICE_ID_REQ` |
| `BL_STATE_DEVICE_ID_REQ` → `BL_STATE_DEVICE_ID_RES` | Request and receive the device ID | `BL_STATE_FW_LENGTH_REQ` |
| `BL_STATE_FW_LENGTH_REQ` → `BL_STATE_FW_LENGTH_RES` | Request and receive the firmware length | `BL_STATE_ERASE_APP` |
| `BL_STATE_ERASE_APP` | Erase the application flash region | `BL_STATE_RECEIVE_FW` |
| `BL_STATE_RECEIVE_FW` | Receive and flash the image chunk by chunk | `BL_STATE_DONE` |

Any unexpected packet at a request/response step, or a timeout while waiting, aborts the update
and the bootloader falls back to waiting at sync.

---
### Packet Protocol

A custom packet protocol handles communication between the STM32 board and the host (PC) over
UART, giving the update process a reliable, acknowledgement/retransmission-based transport instead
of relying on a raw, unverified byte stream.

---
### Two Layers of Integrity Checking

Before the bootloader ever trusts and jumps into a received image, it checks integrity at two
separate levels:

- **Transport layer** — every packet carries a CRC8. A mismatch on either side is caught as soon
  as that packet arrives, catching corruption introduced during the UART transfer itself.
- **Image layer** — once an image is fully received, the bootloader checks a structured firmware
  header (`firmware_info_t`) prepended to it — sentinel value, target device ID, version, length,
  and CRC32 — so it can reject an image that wasn't built for this device, is truncated, or
  doesn't match the expected device ID. The CRC32 in that header is computed over the
  application's code region and is verified by the bootloader before it will ever jump to the new
  application.

---
### Does the Firmware Actually Work?

Checking these two layers only confirms that the received *bytes* are intact — it says nothing
about whether the new firmware actually runs correctly once the bootloader jumps into it. To guard
against that, a **fail-safe recovery mechanism** is used, built around its own boot-status state
machine. 
- **Reset-survivable boot status via backup-domain registers** — a status word (erased /
  `PENDING` / `CONFIRMED`) is written to a register in the backup domain, which is preserved
  across a warm reset. This is what lets the bootloader tell "this app has never proven itself"
  apart from "this app is known-good," even after a reset wipes SRAM.
- **Watchdog-gated fail-safe recovery** — the bootloader marks the app `PENDING` and arms the
  independent watchdog (IWDG) *before* jumping. The application must explicitly write `CONFIRMED`
  and then keep feeding the watchdog. If it crashes, hangs, or never gets that far, the IWDG
  resets the chip — and on the next boot the status is still `PENDING`, so the bootloader refuses
  to jump into the same bad image again and instead waits for a new update over UART.

State Machine:
| Boot status (backup-domain register) | Bootloader's decision |
|---|---|
| Erased / `BOOT_MAGIC_CONFIRMED` | Mark `PENDING`, arm IWDG, jump to app |
| `BOOT_MAGIC_PENDING` / `BOOT_MAGIC_FAILED` | Stay resident, wait for a new update over UART |

---

## 3. Memory Layout

```
0x08000000 ┌───────────────────────────────┐
           │  mcu-bootloader (padded)      │
0x08008000 ├───────────────────────────────┤
           │  firmware_info_t header (36 B)│
0x08008024 ├───────────────────────────────┤  (padded to 0x08008080)
           │  Vector table (0x80-aligned)  │
           ├───────────────────────────────┤
           │  mcu-app code + data (96 KB)  │
0x08020000 └───────────────────────────────┘  end of 128 KB flash
```

- The bootloader region (`0x08000000`–`0x08008000`) isn't fully occupied by bootloader code — the
  actual binary only takes up part of it, and the remaining space is padded with `0xFF` bytes by a
  Python post-build script (`pad-bootloader.py`), so the bootloader image is always exactly
  `0x8000` bytes.
- The padding after the firmware header exists to force the application's vector table onto a
  128-byte (`0x80`) boundary, per ARMv6-M VTOR alignment requirements.

---

## 4. Hardware Used

- **MCU:** STM32G070RB (Arm Cortex-M0+, 128 KB flash, 36 KB SRAM)
- **Programmer/debugger:** ST-Link (any V2/V2.1-compatible probe)
- **Host-to-MCU link:** USB-UART adapter (or the ST-Link's onboard VCP) wired to the MCU's USART
  pins, 115200 baud
- Runs on a normal Windows/Linux laptop — no additional host-side hardware needed

---

## 5. Toolchain, Installations, and Dependencies

### Embedded side (both `mcu-bootloader` and `mcu-app`)
- **ARM GNU Toolchain** (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, `arm-none-eabi-objdump`,
  `arm-none-eabi-size`) — tested with 15.x
- **GNU Make**
- **OpenOCD** with the `interface/stlink.cfg` and `target/stm32g0x.cfg` configs, for flashing and
  debugging over ST-Link
- **Python 3** — used by `mcu-bootloader/pad-bootloader.py` as a post-build step
- On Windows: **MSYS2/mingw64** is the recommended environment for the above, plus **Zadig** to
  install the WinUSB driver for the ST-Link if OpenOCD can't see it
- *(Optional)* VS Code with the **Cortex-Debug** extension — `.vscode/launch.json` and
  `tasks.json` are already set up for it

> Shared peripheral drivers (UART, ring buffer, flash, CRC, SysTick, GPIO/clock config, a simple
> software timer) live in `Common_Drivers/` at the repo root and are used by both
> `mcu-bootloader` and `mcu-app` via `../Common_Drivers/{inc,src}`. No external setup needed — a
> fresh clone of this repo builds on its own.

### Host updater side (`pc-updater`)
- **Node.js** (LTS)
- **TypeScript 5.x** — required specifically; TypeScript 7's rewritten Go-based compiler breaks
  `ts-node`'s programmatic API, so pin to the 5.x line
- **`ts-node`**
- **`serialport`** (npm package) — imported in `index.ts` but not currently declared in
  `package.json`; install manually:
  ```bash
  npm install serialport
  npm install --save-dev typescript ts-node @types/node
  ```

---

## 6. Build & Flash

### Bootloader
```bash
cd mcu-bootloader
make            # produces main.elf, main.bin (padded to exactly 0x8000 bytes)
make flash      # flashes main.elf via OpenOCD + ST-Link
```

### Application
```bash
cd mcu-app
make            # produces app.elf, app.bin
make flash      # flashes app.bin at 0x08008000 via OpenOCD + ST-Link
```

Both Makefiles also support:
```bash
make size    # print .text/.data/.bss sizes
make debug   # start OpenOCD in halt mode for attaching a debugger
make clean
```

> First-time setup only: flash the bootloader once via `make flash` in `mcu-bootloader`, then
> flash an initial application once the same way in `mcu-app`. After that, application updates
> happen over UART via `pc-updater` — no debug probe required.

---

## 7. Steps to Perform a Firmware Update

1. Build a new `mcu-app` image (`make` in `mcu-app`) and copy the resulting `app.bin` into
   `pc-updater/` as `application.bin`.
2. In `pc-updater/index.ts`, set `serialPath` to the COM port / `/dev/tty*` device your UART
   adapter enumerates as.
3. Install dependencies (see §5) and run the updater:
   ```bash
   cd pc-updater
   npx ts-node index.ts
   ```
4. The updater will:
   - Patch the firmware header in-memory (version, length, CRC32) before sending anything
   - Send the 4-byte sync sequence until the bootloader responds
   - Request a firmware update, exchange device ID and firmware length
   - Wait for the bootloader to erase the application region
   - Stream the image in 16-byte chunks, retransmitting on any CRC or ACK failure
   - Wait for an explicit "update successful" confirmation
5. On the next reset (or automatically, depending on how the bootloader's main loop exits), the
   bootloader validates the header sentinel, device ID, and CRC32, marks the app `PENDING`, arms
   the watchdog, and jumps. If the new application confirms itself and keeps feeding the watchdog,
   it's now the trusted image; if it doesn't, the device recovers back into update-wait mode
   automatically on the next reset.

---

## 8. Repository Structure

```
stm32-bootloader/
├── Common_Drivers/           # Shared low-level drivers used by both mcu-bootloader and mcu-app
│   ├── inc/                  #   uart.h, ringbuffer.h, flash.h, crc.h, chip_config_init.h, ...
│   └── src/
├── mcu-bootloader/          # The bootloader (0x08000000, first 32 KB)
│   ├── src/                 #   state machine, comms, startup
│   ├── inc/                 #   comms.h (packet protocol definitions)
│   ├── pad-bootloader.py    #   post-build: pads binary to exactly 0x8000 bytes
│   └── stm32g070.ld         #   linker script for the bootloader region
├── mcu-app/                 # The application (0x08008000, remaining 96 KB)
│   ├── src/                 #   main.c, app_header.c (firmware header), startup
│   ├── inc/
│   └── stm32g070.ld         #   linker script: header + 0x80-aligned vector table + app
├── shared/inc/               # Headers shared by bootloader AND application
│   ├── firmware_info.h      #   firmware header struct, flash layout constants
│   └── boot_status.h        #   backup-register boot-status contract, IWDG register defs
├── pc-updater/               # Host-side updater (Node.js / TypeScript)
│   ├── index.ts              #   packet protocol + update sequence, mirrors the firmware
│   └── package.json
└── .vscode/                  # Cortex-Debug launch/build configuration
```

> Earlier, incremental development stages of this project (working through the packet protocol,
> flash driver, and recovery logic one piece at a time) are preserved on the
> [`archive/dev-stages`](../../tree/archive/dev-stages) branch rather than cluttering `main`.

---

## 9. References

- RM0454 — STM32G0x0 reference manual (flash, IWDG, backup domain/TAMP registers)
- PM0223 — Arm Cortex-M0+ programming manual (VTOR alignment requirement)
- AN3155 / AN4221 — ST's own USART/I2C bootloader protocol app notes (used as a conceptual point
  of comparison while designing the custom packet protocol here, not implemented directly)
- [MCUboot documentation](https://docs.mcuboot.com/) — read as a reference design for image-slot
  and fail-safe update strategies while designing this project's simpler single-slot approach

---

## Known Limitations & Future Improvements

- **`pc-updater/package.json` doesn't declare its runtime dependency** on `serialport`, or its
  dev dependencies on `typescript`/`ts-node`. *Planned fix:* run `npm install` for the packages
  actually used and commit the resulting `package.json`/lockfile.
- **Serial port and device ID are hardcoded** in `pc-updater/index.ts` (`COM5`, and a `DEVICE_ID`
  that isn't currently assigned before use). *Planned fix:* accept these as CLI arguments or a
  small config file.
- **Single-slot recovery, not full A/B rollback.** The current design can detect and reject a bad
  update (fall back to update-wait mode) but cannot automatically restore the *previous working*
  application image, since there is only one application slot. A dual-slot or delta-patch scheme
  (as in MCUboot or the referenced `stm32-secure-patching-bootloader`) would add that at the cost
  of flash space and swap complexity.
- **`BOOT_MAGIC_FAILED` is defined but currently unused.** `boot_status.h` reserves a magic value
  for an explicit "known-bad" status, intended to be written from a fault handler (e.g. a HardFault
  or a caught application-level error), but no such handler currently writes it. Wiring this up
  would let the system distinguish "crashed hard" from "just never confirmed in time."
- **No retry-counter / escalating recovery.** A second backup register could track how many
  consecutive updates have failed to confirm, so that after N consecutive bad updates the device
  could take a different action (e.g. hold in a distinct "needs manual recovery" state) rather than
  waiting indefinitely for another UART update.
- **No image encryption or signing.** The CRC32 check protects against corruption and truncation,
  not against a deliberately malicious image — there's no cryptographic signature check before the
  bootloader trusts an image's contents.
- **No demo capture yet.** A short terminal/GIF capture of a full update — sync, transfer,
  validation, jump, confirmation — would make the update flow immediately legible to someone
  skimming the repo instead of reading the protocol description above.

---

## License

MIT — see `LICENSE`.
