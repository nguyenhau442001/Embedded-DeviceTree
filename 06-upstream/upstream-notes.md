# Phase 6 — Upstream Contribution

## Finding a Real Gap

Before writing a patch, find something genuinely missing or broken. Good entry points:

### 1. Missing binding documentation
Many older drivers have a `txt` binding or no binding at all. Converting a `.txt` binding to YAML is a well-accepted first contribution.

```bash
# Find txt bindings that have no YAML equivalent yet
ls Documentation/devicetree/bindings/ | grep "\.txt$"

# Check if a YAML version exists
ls Documentation/devicetree/bindings/*/vendor,device.yaml 2>/dev/null
```

### 2. Undocumented properties
A driver uses `of_property_read_*` for a property that isn't in any binding doc.

```bash
# Find properties read by a driver with no binding entry
grep -r "of_property_read" drivers/misc/mydriver.c | grep -oP '"[a-z-]+"'
# Then check if those property names appear in the binding YAML
```

### 3. `dtbs_check` failures
Run `dtbs_check` on an upstream board DTS and look for schema violations:

```bash
# From kernel source root on the Pi or a cross-compile host
make dtbs_check 2>&1 | grep "^arch/.*\.dts"
```

### 4. Browse the mailing list for open review threads
Patches sometimes sit with minor review comments unaddressed. Picking one up and reposting with fixes is a valid contribution.

- [linux-devicetree archive](https://lore.kernel.org/linux-devicetree/)

---

## Workflow: From Patch to Merge

```
1. Clone mainline kernel
2. Find the gap (binding, DTS fix, schema error)
3. Write the patch
4. Run local validation (dtbs_check, checkpatch, get_maintainer)
5. Send to mailing list
6. Address review feedback
7. Maintainer picks up patch → merged via devicetree/next → Linus pull
```

---

## Setting Up the Kernel Source

```bash
# Clone Linus's tree (large — use --depth for speed)
git clone --depth=1 https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
cd linux

# Or clone the DT maintainer's tree (where DT patches land first)
git clone https://git.kernel.org/pub/scm/linux/kernel/git/robh/linux.git
cd linux
git checkout dt/next
```

---

## Writing a YAML Binding Patch

### File location
```
Documentation/devicetree/bindings/<subsystem>/vendor,device.yaml
```

### Patch checklist
- [ ] `$id` URL matches the file path exactly
- [ ] `$schema` points to `core.yaml#`
- [ ] `title` is one line, no trailing period
- [ ] `maintainers` list has at least one active email
- [ ] All properties used by the driver are documented
- [ ] `required:` lists only truly mandatory properties
- [ ] `additionalProperties: false` (or `unevaluatedProperties: false` if using allOf)
- [ ] `examples:` section compiles cleanly with `dtc`

### Validate before sending
```bash
# Install dtschema
pip3 install dtschema

# Validate your binding file itself
dt-validate -m Documentation/devicetree/bindings/misc/vendor,mydev.yaml

# Validate all DTS files against your new schema
make dtbs_check DT_SCHEMA_FILES=Documentation/devicetree/bindings/misc/vendor,mydev.yaml
```

---

## `get_maintainer.pl` — Who to CC

Always run this before sending. It reads `MAINTAINERS` and git history to produce the exact To/CC list.

```bash
./scripts/get_maintainer.pl Documentation/devicetree/bindings/misc/vendor,mydev.yaml
# Output:
# Rob Herring <robh+dt@kernel.org> (maintainer:OPEN FIRMWARE AND FLATTENED DEVICE TREE BINDINGS)
# Krzysztof Kozlowski <krzk+dt@kernel.org> (maintainer:...)
# devicetree@vger.kernel.org (open list:OPEN FIRMWARE...)
# linux-kernel@vger.kernel.org (open list)
```

For a DTS fix also run it on the DTS file:
```bash
./scripts/get_maintainer.pl arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts
```

---

## `checkpatch.pl` — Style Validation

```bash
# Check a generated patch file
./scripts/checkpatch.pl 0001-dt-bindings-add-vendor-mydev.patch

# Check staged changes directly
git diff HEAD | ./scripts/checkpatch.pl --no-tree -
```

Fix all `ERROR:` and `WARNING:` lines before sending. `CHECK:` items are optional but worth addressing.

---

## Sending the Patch via `git send-email`

### Configure git send-email (one time)
```bash
git config --global sendemail.smtpserver smtp.gmail.com
git config --global sendemail.smtpserverport 587
git config --global sendemail.smtpencryption tls
git config --global sendemail.smtpuser you@gmail.com
```

### Format the patch
```bash
git format-patch -1 HEAD \
    --subject-prefix="PATCH" \
    --cover-letter    # only for series of 2+ patches
```

Edit the cover letter (`0000-cover-letter.patch`) for a series. For a single patch, the commit message is the cover letter.

### Send
```bash
git send-email \
    --to="robh+dt@kernel.org" \
    --cc="krzk+dt@kernel.org" \
    --cc="devicetree@vger.kernel.org" \
    --cc="linux-kernel@vger.kernel.org" \
    0001-dt-bindings-add-vendor-mydev.patch
```

### Sending a v2 after review feedback
```bash
git format-patch -1 HEAD \
    --subject-prefix="PATCH v2"

# Add a changelog below the --- line in the patch:
# ---
# Changes in v2:
# - Fixed additionalProperties per Krzysztof's review
# - Added missing clock-names property
```

---

## Commit Message Format for DT Patches

```
dt-bindings: <subsystem>: Add binding for vendor,mydev

Add a YAML schema for the Vendor MyDev peripheral, a SPI-attached
sensor used on several Broadcom-based boards.

Signed-off-by: Your Name <you@example.com>
```

Rules:
- Subject prefix: `dt-bindings:` for binding docs, `arm64: dts:` for DTS files
- 72 character line limit in body
- `Signed-off-by` is mandatory (DCO — Developer Certificate of Origin)
- No period at end of subject line

---

## Addressing Review Feedback

The DT maintainers (Rob Herring, Krzysztof Kozlowski) review binding patches thoroughly. Common feedback:

| Feedback | What to do |
|---|---|
| "use `unevaluatedProperties` instead of `additionalProperties`" | When using `allOf`/`$ref` to inherit a base schema |
| "missing `clock-names` when `clocks` is present" | Add `clock-names` with an `items:` list matching each clock |
| "`required` should not list optional properties" | Move conditional properties to `if/then` or just document them |
| "`examples` does not compile" | Run `dtc -I dts -O dtb` on the example block and fix errors |
| "get_maintainer missed X" | Re-run `get_maintainer.pl` and add the missing address in v2 |

Reply to the review email inline (not top-post), addressing each point. Send v2 as a fresh `git send-email`, not a reply to the thread.

---

## Tracking Your Patch

```bash
# Search the mailing list archive
# https://lore.kernel.org/linux-devicetree/?q=vendor%2Cmydev

# Check if it landed in robh/dt-next
git fetch https://git.kernel.org/pub/scm/linux/kernel/git/robh/linux.git dt/next
git log FETCH_HEAD --oneline | grep "vendor,mydev"

# Check if it's in mainline
git fetch https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git master
git log FETCH_HEAD --oneline | grep "vendor,mydev"
```

---

## Submitted Patches Log

Track your own submissions here as you go:

| Patch | Mailing list link | Status | Notes |
|---|---|---|---|
| _(none yet)_ | | | |

---

## References

- [Submitting patches — kernel docs](https://www.kernel.org/doc/html/latest/process/submitting-patches.html)
- [Writing DT bindings in YAML](https://www.kernel.org/doc/html/latest/devicetree/bindings/writing-schema.html)
- [linux-devicetree mailing list](https://lore.kernel.org/linux-devicetree/)
- [dtschema](https://github.com/devicetree-org/dt-schema)
- [DT maintainer tree (robh)](https://git.kernel.org/pub/scm/linux/kernel/git/robh/linux.git)
