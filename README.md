# Embedded-DeviceTree — Mastery Roadmap

Goal: master the Linux kernel Device Tree subsystem end-to-end — not just DTS syntax, but the full chain from boot handoff to driver binding to upstream contribution. Oriented toward the Android Framework Engineer / AOSP HAL-layer track.

## Progress tracker

- [ ] [Phase 0 — ARM boot flow & DT fundamentals](#phase-0--arm-boot-flow--dt-fundamentals)
- [ ] [Phase 1 — DT syntax & tooling](#phase-1--dt-syntax--tooling)
- [ ] [Phase 2 — Driver binding](#phase-2--driver-binding)
- [ ] [Phase 3 — Real hardware & debugging](#phase-3--real-hardware--debugging)
- [ ] [Phase 4 — Media/display topology](#phase-4--mediadisplay-topology)
- [ ] [Phase 5 — AOSP integration](#phase-5--aosp-integration)
- [ ] [Phase 6 — Upstream contribution](#phase-6--upstream-contribution)

---

## Phase 0 — ARM boot flow & DT fundamentals
**Time estimate:** 3–4 days

**Goal:** Understand exactly where and how the device tree enters the ARM boot chain, and why DT replaced the old board-file approach.

**Topics to cover:**
- Boot chain stages: BootROM → SPL → U-Boot → kernel handoff
- Where the DTB physically enters the chain (loaded by U-Boot, address passed to the kernel, unflattened into the live device tree)
- ARM32 ATAGS vs DT handoff (historical context, why DT replaced it)
- Board file (pre-2011, hardcoded `arch/arm/mach-*/board-*.c`) vs DT-described platform (declarative `.dts`, one kernel Image + many DTBs)
- Why the kernel community forced this switch (the board-file explosion problem)

**Repo milestone:** `00-foundations/boot-flow-notes.md` — a boot chain diagram plus a short writeup comparing one real upstream pre-DT board file against its modern DT-based equivalent.

**Documents:**
- [ ] [rpi-setup.md](00-foundations/rpi-setup.md)
- [ ] [cross-compile-setup.md](00-foundations/cross-compile-setup.md)
- [ ] [boot-flow-notes.md](00-foundations/boot-flow-notes.md)

---

## Phase 1 — DT syntax & tooling
**Time estimate:** 1 week

**Topics:**
- `.dts`/`.dtsi` syntax: nodes, properties, phandles, `#address-cells`/`#size-cells`, `aliases`, `/chosen`
- Compiling with `dtc`, decompiling a real `.dtb` back to source
- Device tree overlays (`.dtbo`) — compile and apply at runtime

**Repo milestone:** `01-syntax/` — decompile a real shipped `.dtb` (e.g. Raspberry Pi 4 or BeagleBone Black), diff against upstream source, document what changed and why.

**Documents:**
- [ ] [syntax-notes.md](01-syntax/syntax-notes.md)

---

## Phase 2 — Driver binding
**Time estimate:** 2 weeks

**Topics:**
- `platform_driver` + `of_match_table` registration
- `of_property_read_*`, `of_get_named_gpio`, `devm_*` resource management
- Writing a binding in the modern YAML schema format, validating with `dtschema`/`dt-validate`
- Clock, pinctrl, regulator consumer bindings

**Repo milestone:** `02-custom-binding/` — invent a fake peripheral, write its YAML binding, write the platform driver that probes it via DT, get it loading on real hardware or QEMU.

**Documents:**
- [ ] [binding-notes.md](02-custom-binding/binding-notes.md)

---

## Phase 3 — Real hardware & debugging
**Time estimate:** 2–3 weeks

**Target hardware:** Raspberry Pi 4/5 (strong upstream DT support) or QEMU `virt`/`vexpress` (no hardware cost)

**Topics:**
- Reading `/proc/device-tree` and `/sys/firmware/devicetree/base` on a live system
- Diagnosing boot failures from DT errors (missing `compatible`, wrong `reg`, phandle resolution failures)
- `of_node` refcounting bugs
- `dtc -@` symbol mode for overlay phandle resolution

**Repo milestone:** `03-debugging/` — deliberately break 5 things in a DTS (bad reg, missing clock, wrong compatible, overlay conflict, phandle loop), document the exact boot-log signature and fix for each.

**Documents:**
- [ ] [debugging-notes.md](03-debugging/debugging-notes.md)

---

## Phase 4 — Media/display topology
**Time estimate:** 1–2 weeks

**Topics:**
- `of_graph` bindings — how camera sensors, display panels, bridge chips describe pipeline topology (`port`, `endpoint`, `remote-endpoint`)
- How V4L2/DRM drivers consume this at probe time

**Repo milestone:** `04-media-topology/` — diagram and annotate a real upstream display pipeline DTS, mapping graph properties to the driver probe sequence.

**Documents:**
- [ ] [media-topology-notes.md](04-media-topology/media-topology-notes.md)

---

## Phase 5 — AOSP integration
**Time estimate:** 1–2 weeks

**Topics:**
- How Android's kernel build integrates DT (vendor DLKM, `ANDROIDBOOT.*` params via `/chosen`, vendor boot image handling)
- Where DT-described hardware surfaces into HAL (DT-probed driver → sysfs/uevent → HAL)
- Soong/Bazel handling of DTB/DTBO build targets

**Repo milestone:** `05-aosp-integration/` — trace one real example end-to-end (DT node → kernel driver → sysfs node → HAL consumption) using a public AOSP reference device tree.

**Documents:**
- [ ] [aosp-integration-notes.md](05-aosp-integration/aosp-integration-notes.md)

---

## Phase 6 — Upstream contribution
**Time estimate:** ongoing, start month 2–3

**Topics:**
- Finding a real gap: missing binding, undocumented property, bug in an existing binding doc
- Submitting a patch to the `linux-devicetree` mailing list (`get_maintainer.pl`, `checkpatch.pl`, SPDX headers, `dtbs_check` passing in CI)

**Repo milestone:** `06-upstream/` — link submitted patch(es), mailing list thread, and notes on maintainer feedback.

**Documents:**
- [ ] [upstream-notes.md](06-upstream/upstream-notes.md)

---

## Repo structure

```
Embedded-DeviceTree/
├── README.md
├── 00-foundations/
│   ├── rpi-setup.md
│   ├── cross-compile-setup.md
│   └── boot-flow-notes.md
├── 01-syntax/
│   └── syntax-notes.md
├── 02-custom-binding/
│   └── binding-notes.md
├── 03-debugging/
│   └── debugging-notes.md
├── 04-media-topology/
│   └── media-topology-notes.md
├── 05-aosp-integration/
│   └── aosp-integration-notes.md
└── 06-upstream/
    └── upstream-notes.md
```