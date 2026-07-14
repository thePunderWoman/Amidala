# Vendored browser dependencies

Third-party libraries the Firmware Installer Wizard loads at runtime, committed
here and served **locally** instead of from a CDN. This gives the flasher
**zero external runtime dependencies** — it works offline and is immune to CDN
outages/changes.

> Same rationale as [Wireless_Communication_Board-WCB's Wizard/vendor](https://github.com/greghulette/Wireless_Communication_Board-WCB/tree/main/Wizard/vendor),
> which this wizard's flashing code is modeled after — self-hosting removes an
> entire class of "Failed to fetch dynamically imported module" failures that
> CDN redirect quirks can cause mid-flash.

## Contents

| Path | Library | Version | Upstream source |
|------|---------|---------|-----------------|
| `esptool-js/esptool-js-0.4.7.bundle.js` | [esptool-js](https://github.com/espressif/esptool-js) | 0.4.7 | `https://unpkg.com/esptool-js@0.4.7/bundle.js` |
| `crypto-js/crypto-js-4.2.0.min.js` | [crypto-js](https://github.com/brix/crypto-js) | 4.2.0 | `https://cdnjs.cloudflare.com/ajax/libs/crypto-js/4.2.0/crypto-js.min.js` |

`esptool-js-0.4.7.bundle.js` is the project's official **self-contained ESM
bundle**: pako (zlib) and every chip-target ROM are inlined, so it has **no
dynamic `import()`s and no external fetches**. It exports `ESPLoader`,
`Transport`, etc., consumed by [`../flasher.js`](../flasher.js). CryptoJS is
the MD5 provider esptool-js needs for flash verification.

Both are referenced by relative paths in `flasher.js`, which resolve correctly
under the GitHub Pages subpath (`/Amidala/wizard/firmware-installer/`).

## Updating a version

1. Download the new **pinned, self-contained** build, e.g.
   ```
   curl -L https://unpkg.com/esptool-js@<ver>/bundle.js \
     -o esptool-js/esptool-js-<ver>.bundle.js
   ```
2. Sanity-check it's truly self-contained — these must all return nothing:
   ```
   grep -aoE 'import\(|require\("|from"[^.]' esptool-js/esptool-js-<ver>.bundle.js
   ```
   and it must end with `export{... ESPLoader ... Transport ...}`.
3. Update the version in the path constants at the top of `../flasher.js`
   (`ESPTOOL_SRC` / `CRYPTOJS_SRC`) and the table above.
4. Remove the old version file.
