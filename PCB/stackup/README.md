# Amidala Shield Stackup design

`board.kdl` is the shield's electrical design. It imports parts from the
[Stackup library](https://github.com/stackup-eda/library) pinned in the repository's
`manifest.kdl`. `parts.kdl` defines the parts the library doesn't have: the ESP32-S3-DevKitC-1,
the Hirose microSD socket, the 5 V screw terminal and the 3-way supply jumper.

The design was ported 1:1 from the KiCad schematics. Designators and net names are unchanged, so
the existing routing in `../AmidalaShield.kicad_pcb` carried over as-is.

From the repository root:

```sh
stackup check PCB/stackup/board.kdl --locked     # check the design
```

To update the PCB after changing the design, use **Sync PCB from stackup** in KiCad's PCB editor
(the board is linked by `PCB/AmidalaShield.stackup_sch`), or sync it headless with the board
closed:

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
  <stackup checkout>/kicad-plugin/sync_headless.py PCB/AmidalaShield.kicad_pcb
```

Commit the synced PCB together with the KDL change. CI checks the design with the Cargo-locked CLI
in `ci/stackup/` and fails if syncing would change the committed PCB. Each footprint records the
`board.kdl` line it came from, so even a change that only moves lines needs a resync.

Install the same CLI version CI uses with `cargo install stackup-eda --version 0.2.0 --locked`, and
the KiCad plugin from a checkout of [stackup-eda/stackup](https://github.com/stackup-eda/stackup)
at `v0.2.0` with `python3 kicad-plugin/install.py`.

## Known gaps carried over from the schematics

- **No MPNs.** The original schematics didn't specify purchasing details, so `stackup bom` reports
  every part as missing an MPN.
- **XBee3 SPI_ATTN has no pull-up.** `stackup check` reports a note that the strap's rest level
  can't be checked. The fix is a resistor in the next board revision (issue #185).
