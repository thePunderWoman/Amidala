// flasher.js
// Web Serial flashing for the Amidala Firmware Installer Wizard.
//
// Uses esptool-js (vendored — see vendor/README.md) to write a single merged
// image to offset 0x0 over Web Serial. CI (.github/workflows/
// build-firmware-matrix.yml) produces that image per drive/dome/board combo
// from PlatformIO's own firmware.factory.bin (bootloader + partition table +
// app, contiguous from 0x0) — no separate merge step needed here.
//
// Modeled after greghulette/Wireless_Communication_Board-WCB's
// Wizard/flasher.js, simplified: this is a first-flash/factory-reset
// installer (always writes the full image), and Amidala only targets one
// chip family (ESP32-S3), so there's no multi-family bootloader selection,
// partition-migration detection, or app-only update path to carry over.

'use strict';

// Resolved relative to THIS script's own URL so it works under the GitHub
// Pages subpath, same reasoning as WCB's flasher.js.
const _VENDOR_BASE = (() => {
  try {
    const u = document.currentScript && document.currentScript.src;
    if (u) return new URL('vendor/', u).href;
  } catch (_) { /* fall through */ }
  return './vendor/';
})();
const ESPTOOL_SRC  = _VENDOR_BASE + 'esptool-js/esptool-js-0.4.7.bundle.js';
const CRYPTOJS_SRC = _VENDOR_BASE + 'crypto-js/crypto-js-4.2.0.min.js';

function loadScript(src) {
  return new Promise((resolve, reject) => {
    if (document.querySelector(`script[src="${src}"]`)) { resolve(); return; }
    const s = document.createElement('script');
    s.src = src;
    s.onload = resolve;
    s.onerror = () => reject(new Error(`Failed to load script: ${src}`));
    document.head.appendChild(s);
  });
}

// esptool-js requires flash data as a Latin1 string, not a Uint8Array.
function bufToLatin1(buf) {
  const u8 = new Uint8Array(buf);
  let s = '';
  const CHUNK = 65536; // avoid call-stack overflow on large binaries
  for (let i = 0; i < u8.length; i += CHUNK) {
    s += String.fromCharCode.apply(null, u8.subarray(i, i + CHUNK));
  }
  return s;
}

// Board module -> expected flash size in MB. The merged image's baked-in
// bootloader declares a flash size in its header; writing one built for the
// wrong size silently corrupts NVS on first boot, so this is checked before
// any write happens.
const BOARD_FLASH_MB = { n16r8: 16, n8r2: 8 };

async function detectFlashSizeMB(loader) {
  try {
    const kb = await loader.getFlashSize();
    if (Number.isFinite(kb) && kb > 0) return kb / 1024;
  } catch (_) { /* fall through to raw flash-ID read */ }
  try {
    const flashId = await loader.readFlashId();
    const szId = (flashId >> 16) & 0xFF; // bits 16-23 = log2(size in bytes)
    if (szId >= 0x12 && szId <= 0x19) return (1 << szId) / (1024 * 1024);
  } catch (_) { /* undetectable */ }
  return null;
}

// port      — a WebSerial SerialPort from navigator.serial.requestPort(), not yet open.
// board     — 'n16r8' | 'n8r2', the module selected in Step 1.
// imageBuf  — ArrayBuffer of the fetched amidala-*.bin release asset.
// callbacks — { onProgress(written, total), onLog(msg), onStatus(msg) }
//
// Throws on error; caller is responsible for UI cleanup.
async function flashFirmware(port, board, imageBuf, { onProgress, onLog, onStatus }) {
  onStatus('Loading flash tool…');

  onLog('Loading CryptoJS…');
  try {
    await loadScript(CRYPTOJS_SRC);
  } catch (e) {
    throw new Error(`Could not load CryptoJS (vendor/crypto-js) — try a hard refresh.\n${e.message}`);
  }

  onLog('Loading esptool-js…');
  let ESPLoader, Transport;
  try {
    ({ ESPLoader, Transport } = await import(ESPTOOL_SRC));
  } catch (e) {
    throw new Error(`Could not load esptool-js (vendor/esptool-js) — try a hard refresh.\n${e.message}`);
  }

  onStatus('Connecting to bootloader…');
  onLog('Connecting to ESP32-S3 bootloader…');
  onLog('► If this hangs: hold BOOT, tap RESET, release BOOT, then try again.');

  // Routes esptool-js's internal sync/progress logging into our log panel.
  const terminal = {
    clean: () => {},
    writeLine: (msg) => { if (msg && msg.trim()) onLog(`[esptool] ${msg.trim()}`); },
    write:     (msg) => { if (msg && msg.trim()) onLog(`[esptool] ${msg.trim()}`); },
  };

  const transport = new Transport(port, false);
  const loader = new ESPLoader({
    transport,
    baudrate: 460800,
    romBaudrate: 115200,
    enableTracing: false,
    terminal,
  });

  let chip;
  try {
    chip = await loader.main();
    onLog(`Chip identified: ${chip}`);
  } catch (e) {
    try { await transport.disconnect(); } catch (_) {}
    throw new Error(
      `Bootloader connection failed: ${e.message}\n\n` +
      `Hold the BOOT button, tap RESET, release BOOT, then click Connect again.`
    );
  }

  // ESP32-S3 has no auto-reset circuitry on its native USB-CDC port (that
  // trick needs a separate USB-UART bridge chip this board doesn't expose
  // here), so loader.main() above only succeeds if the board was already
  // manually put into bootloader mode — hence the instruction above.

  const chipName = (loader.chip && loader.chip.CHIP_NAME) || String(chip || '');
  if (!/ESP32[-_]?S3/i.test(chipName)) {
    try { await transport.disconnect(); } catch (_) {}
    throw new Error(
      `Detected "${chipName}", but Amidala only runs on ESP32-S3 — refusing to flash. ` +
      `Make sure you're connected to the right board.`
    );
  }
  onLog('Chip family confirmed: ESP32-S3');

  const flashSizeMB = await detectFlashSizeMB(loader);
  const expectedMB = BOARD_FLASH_MB[board];
  if (!flashSizeMB) {
    try { await transport.disconnect(); } catch (_) {}
    throw new Error(
      "Could not detect the board's flash size — refusing to flash. " +
      'A mismatched bootloader silently corrupts saved settings on boot.'
    );
  }
  if (flashSizeMB !== expectedMB) {
    try { await transport.disconnect(); } catch (_) {}
    throw new Error(
      `Detected ${flashSizeMB} MB of flash, but you selected the ${expectedMB} MB ` +
      `(${board.toUpperCase()}) module. Go back and pick the module that matches your ` +
      `board — flashing the wrong size will corrupt boot.`
    );
  }
  onLog(`Flash size confirmed: ${flashSizeMB} MB`);

  onStatus(`Flashing ${chip}…`);
  const totalBytes = imageBuf.byteLength;
  onLog(`Writing ${Math.round(totalBytes / 1024)} KB at offset 0x0…`);
  onLog('⚠️  Do not disconnect the board until flashing completes.');
  onLog('⚠️  This erases all saved settings (WiFi, config, calibration) on the board.');
  onProgress(0, totalBytes);

  const attemptWrite = async () => {
    await loader.writeFlash({
      fileArray: [{ data: bufToLatin1(imageBuf), address: 0x0 }],
      flashSize: 'keep',
      flashMode: 'keep',
      flashFreq: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (_fileIdx, written, total) => onProgress(written, total),
      calculateMD5Hash: (img) => CryptoJS.MD5(CryptoJS.enc.Latin1.parse(img)).toString(),
    });
  };

  try {
    await attemptWrite();
  } catch (e1) {
    onLog(`Flash failed once (${e1.message || e1}) — retrying automatically…`);
    onProgress(0, totalBytes);
    try {
      await attemptWrite();
    } catch (e2) {
      try { await transport.disconnect(); } catch (_) {}
      throw new Error(
        `Flash write failed after retry: ${e2.message}\n\n` +
        `To recover: hold BOOT, tap RESET, release BOOT, then try again.`
      );
    }
  }

  onLog('Resetting board into firmware…');
  onStatus('Resetting…');
  onProgress(totalBytes, totalBytes);
  try { if (loader.afterFlash) await loader.afterFlash('hard_reset'); } catch (_) {}
  try { await transport.disconnect(); } catch (_) {}

  onLog('Flash complete — board rebooting');
  onStatus('Flash complete!');
}
