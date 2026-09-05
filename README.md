<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# AI Passport — Wi-Fi OTA Loader

> **Status: not yet validated on real hardware.** The merged image builds and passes the
> official `tools/verify_firmware.py` gate, but the three checks in
> [Safety checklist](#safety-checklist-must-be-verified-on-real-hardware) are still open.
> Flashing this to a card means re-partitioning it. Do not flash a card you cannot afford to
> recover over USB.

A modified build of [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport) that adds an
over-the-air firmware update channel over Wi-Fi. Once installed on a card, future plays can be
pushed to the device over the local network — no USB cable required after the first flash.

This is **not a play**. It is a system-level firmware modification (partition layout + base
launcher) intended to act as the carrier for any subsequent OTA-pushed firmware.

## What it does

- Adds an `ota_0` application partition at `0x360000` (3 MB) and an `otadata` partition at
  `0x310000` (8 KB). `factory`, `cardid` (`0x356000`), and `recovery` (`0x700000`) are left
  untouched.
- Adds a `Wireless Update` entry to the demo menu. Selecting it writes an RTC flag and reboots
  into OTA mode.
- OTA mode does **not** initialise LVGL. It brings up an AP (`AIPassport-OTA` /
  password `updateme`) and an `esp_http_server` instance at `http://192.168.4.1/`.
- POST an **app image** (`build/FoloToy-AI-Passport.bin`) to `/update` (or use the built-in
  upload page). It is written to the slot the device is *not* running from, the boot partition
  is flipped, and the device reboots into the new image.
- Adds a `Back to Loader` entry. It flips the boot partition back to `factory` and reboots, so
  the OTA-capable loader is always reachable.
- Long-press `UP` for five seconds at any time to enter the official Recovery — the only
  hardware-level un-bricking path. This hook is preserved.

## Why this exists

The official firmware upgrade path is a USB Web-Serial tool on the desktop. There is no
in-the-box way to push a freshly compiled play without plugging in a cable. This branch adds
that capability without depending on any closed-source component.

It does **not** replace the official mini-program BLE install contract — the merged image
still passes `tools/verify_firmware.py`.

## Layout

```
nvs         0x009000  24 KB
phy_init   0x00f000   4 KB
factory    0x010000   3 MB   ← slot A: the loader (this branch lives here)
otadata    0x310000   8 KB   ← NEW, OTA bookkeeping
cardid     0x356000  16 KB   ← protected, device identity
ota_0      0x360000   3 MB   ← slot B: the other app slot
recovery   0x700000   1 MB   ← protected, official Recovery
```

## Build

```bash
. /path/to/esp-idf/export.sh
cd ai-passport
idf.py set-target esp32c3          # first time only
idf.py build
idf merge-bin -o FoloToy-AI-Passport-full.bin
python tools/verify_firmware.py build
```

The last command is the official compatibility gate. The merged `FoloToy-AI-Passport-full.bin`
is the artifact to flash.

> On Windows Git Bash, `idf merge-bin -o build/<file>.bin` will fail with `FileNotFoundError`.
> Run it from the `build/` directory, or use a relative `-o` filename.

## Install

1. **First time — USB is mandatory.** Use the [official Web Flasher](https://ai-passport.folotoy.cn)
   and flash `build/FoloToy-AI-Passport-full.bin` at offset `0x0`. This writes the new partition
   table and creates the `otadata`/`ota_0` slots that OTA needs.
2. After reboot the main menu gains a `Wireless Update` entry.

## Use OTA

1. On the card, navigate to `Wireless Update` and press `OK`. The screen will show the AP
   name and password, then the device reboots into OTA mode.
2. On your phone, connect to the `AIPassport-OTA` Wi-Fi (password `updateme`).
3. Open `http://192.168.4.1/` in the browser and upload **`build/FoloToy-AI-Passport.bin`** —
   the plain app image, **not** `FoloToy-AI-Passport-full.bin`.
4. The card writes the image to the slot it is not running from, flips the boot partition, and
   reboots. The new play starts on the next boot.

> **Upload the app image, not the merged image.** The merged `-full.bin` begins with the
> bootloader; written into an app slot it yields an unbootable partition. The handler rejects
> anything larger than the target partition and anything whose first byte is not the `0xE9`
> ESP image magic.
>
> The recovery and identity partitions are never overwritten.

### Two-slot rotation

8 MB of flash fits only one extra 3 MB app slot, so `factory` doubles as the second slot. The
write target is always the slot the device is *not* running from:

```
running factory (loader)  --OTA-->  write ota_0   --boot-->  play runs from ota_0
running ota_0   (play)    --OTA-->  write factory --boot-->  loader runs from factory
```

`Back to Loader` flips back to `factory` at any time. Since the running partition is never the
write target, OTA works indefinitely — but **everything you push must be built from this
branch** (same partition table, `ota.c` linked in). A play built from the stock tree boots
fine yet has neither `Wireless Update` nor `Back to Loader`; the only way out is the Recovery
hook or USB.

## Safety checklist (must be verified on real hardware)

This branch was developed without access to a physical card. The following three behaviours
must be confirmed before relying on it:

- [ ] With `otadata` freshly erased to `0xFF`, the bootloader falls back to `factory` and the
      device boots normally.
- [ ] Long-pressing `UP` for five seconds still enters official Recovery. This is the only
      hardware-level un-bricking path.
- [ ] A factory reset does not corrupt `otadata` in a way that bricks the card.
- [ ] After one OTA push, `Back to Loader` returns to the loader and the OTA menu still works
      (i.e. the two-slot rotation round-trips).

The official `recovery_boot_hook` is left untouched, so (2) should be fine in principle.

## Limitations

- The Wi-Fi AP has no encryption beyond WPA2-PSK. Anyone with the password can push firmware.
  Do not enable `Wireless Update` in environments you do not trust.
- OTA mode disables LVGL to keep memory headroom for the HTTP transfer. ~400 KB SRAM on
  ESP32-C3 is the binding constraint; pushing a 1.5 MB image takes 20–60 seconds over 802.11b/g.
- Firmware pushed over OTA must be built from this branch. A play compiled from the stock tree
  boots fine, but it has neither `Wireless Update` nor `Back to Loader`; the only way out is
  the Recovery hook (hold `UP` 5 s) or USB.
- This branch is a system-level modification. It is **not** listed on the official play
  community by intent — see "What it is not" below.

## What it is not

- Not a play. It does not contain a game, tool, or interactive experience on its own.
- Not an official feature. FoloToy has not reviewed or blessed this branch.
- Not a replacement for the Recovery hook. If OTA fails, fall back to USB + Recovery.

## Files added / changed

```
partitions.csv            — added otadata + ota_0
main/ota.h                — new, public API
main/ota.c                — new, AP + http server + esp_ota pipeline
main/demo_ota_update.c    — new, menu entry that triggers OTA reboot + Back to Loader page
main/demo.h               — added demo_ota_update_* declarations
main/main.c               — registered demo + wired ota_mode_try_enter()
main/CMakeLists.txt       — added ota.c / demo_ota_update.c, REQUIRES app_update esp_partition esp_http_server
```