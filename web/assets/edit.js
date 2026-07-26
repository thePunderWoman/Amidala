/* Amidala web UI — edit-in-place widget + shared config page helpers.
   Embed script inlines this into every config sub-page.
   In dev mode (scripts/web_dev.py) it's served as /assets/edit.js. */

// ----------------------------------------------------------------- theme ----

function _toggleTheme() {
  var t = document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark';
  document.documentElement.dataset.theme = t;
  localStorage.setItem('amidala-theme', t);
  var btn = document.getElementById('theme-toggle');
  if (btn) btn.textContent = t === 'dark' ? '☀' : '☾';
}

// ------------------------------------------------------------------ toast ---

function showToast(msg, isErr) {
  var t = document.createElement('div');
  t.className = 'toast' + (isErr ? ' toast-err' : '');
  t.textContent = msg;
  document.body.appendChild(t);
  setTimeout(function() { if (t.parentNode) t.parentNode.removeChild(t); }, 2200);
}

// -------------------------------------------------------- edit-in-place -----

function startEdit(btn) {
  var row = btn.closest('.row');
  var inp = row.querySelector('input,select');
  inp.dataset.orig = inp.value;
  row.querySelector('.rv').hidden = true;
  row.querySelector('.ri').hidden = false;
  btn.hidden = true;
  row.querySelector('.bs').hidden = false;
  row.querySelector('.bc').hidden = false;
}

// Hides the edit controls and shows the read-only display again. Does NOT
// touch the input's value -- callers decide separately whether to revert it
// (doCancel) or leave it as-is (doSave, after a successful save).
function _closeEditUI(row) {
  row.querySelector('.rv').hidden = false;
  row.querySelector('.ri').hidden = true;
  row.querySelector('.be').hidden = false;
  row.querySelector('.bs').hidden = true;
  row.querySelector('.bc').hidden = true;
}

function doCancel(btn) {
  var row = btn.closest('.row');
  var inp = row.querySelector('input,select');
  if (inp && inp.dataset.orig !== undefined) inp.value = inp.dataset.orig;
  _closeEditUI(row);
}

async function doSave(btn) {
  var row = btn.closest('.row');
  var key = row.dataset.key;
  var rt  = row.dataset.type;
  var inp = row.querySelector('input,select');
  var val = inp.value;
  var prev = btn.textContent;
  btn.textContent = '…';
  btn.disabled = true;
  if (rt === 'ascii-char') {
    val = val.length > 0 ? String(val.charCodeAt(0)) : '0';
  }
  try {
    var r = await fetch('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'key=' + encodeURIComponent(key) + '&value=' + encodeURIComponent(val)
    });
    if (r.ok) {
      var dv = row.querySelector('.rv');
      if (rt === 'bool' || rt === 'select') {
        var sel = row.querySelector('select');
        dv.textContent = sel.options[sel.selectedIndex].text;
      } else if (rt === 'ascii-char') {
        dv.textContent = _asciiDisp(val);
      } else if (rt === 'password') {
        dv.textContent = '••••••••';
      } else {
        var fmtFn = row.dataset.fmt;
        dv.textContent = (fmtFn && window[fmtFn]) ? window[fmtFn](val) : val;
      }
      // Keep the tracked config snapshot in sync and re-evaluate any other
      // rows whose when: predicate depends on this key, so toggling e.g. a
      // bool field immediately shows/hides its dependent fields without a
      // full page reload.
      if (_configData) _configData[key] = val;
      _applyWhenVisibility();
      _refreshDynamicOptions();
      if (row.dataset.restart === '1') _flagRestartRequired();
      // Optional page-defined hook for keys whose "needs restart" status is
      // stateful rather than a fixed schema property (e.g. WCB identity
      // fields only need a restart if the mesh client is already running --
      // see connectivity.html's refreshWCBStatus()). A plain restart:true
      // flag can't express that, so the page re-checks its own server-side
      // status endpoint here instead of waiting for the next full page load.
      if (window._onConfigSaved) window._onConfigSaved(key);
      // The saved value must stick: update dataset.orig to it (so a future
      // Cancel reverts to this, not the pre-edit value) and close the edit
      // UI directly -- NOT via doCancel(), which would revert inp.value
      // back to the old dataset.orig and silently desync the control from
      // what was actually just saved.
      inp.dataset.orig = val;
      _closeEditUI(row);
      showToast('Saved');
    } else {
      showToast('Save failed: ' + await r.text(), true);
    }
  } catch(e) {
    showToast('Network error', true);
  }
  btn.textContent = prev;
  btn.disabled = false;
}

// ------------------------------------------------ schema-driven row builder --

// Display helper for ascii-char type: number → printable char or named label.
function _asciiDisp(val) {
  var n = parseInt(val, 10);
  if (isNaN(n)) return String(val);
  var names = {0:'NUL', 9:'TAB', 10:'LF', 13:'CR', 27:'ESC', 32:'SP'};
  if (names[n] !== undefined) return names[n];
  if (n > 32 && n < 127) return String.fromCharCode(n);
  return String(n);
}

// s.options may be a static array (most schema rows) or a function(d) that
// computes the option list against the live _configData snapshot -- same
// pattern as s.when(d). Used by pin-picker rows (see pins.html) so their
// choices reflect the server-supplied assignable pool and exclude whatever
// pin every other role currently holds, without duplicating that logic here.
function _resolveOptions(s) {
  if (typeof s.options === 'function') return s.options(_configData) || [];
  return s.options || [];
}

function dispValue(s, val) {
  if (s.type === 'bool') return val === 'y' ? 'On' : 'Off';
  if (s.type === 'select') {
    var found = _resolveOptions(s).find(function(op) { return op.v === String(val); });
    return found ? found.l : val;
  }
  if (s.type === 'password') return '••••••••';
  if (s.type === 'ascii-char') return _asciiDisp(val);
  if (s.fmtFn && window[s.fmtFn]) return window[s.fmtFn](val);
  return String(val);
}

function buildInput(s, val) {
  if (s.type === 'bool') {
    return '<select>'
      + '<option value="y"' + (val === 'y' ? ' selected' : '') + '>On</option>'
      + '<option value="n"' + (val === 'n' ? ' selected' : '') + '>Off</option>'
      + '</select>';
  }
  if (s.type === 'select') {
    var opts = _resolveOptions(s).map(function(op) {
      return '<option value="' + op.v + '"' + (String(val) === op.v ? ' selected' : '') + '>' + op.l + '</option>';
    }).join('');
    return '<select>' + opts + '</select>';
  }
  if (s.type === 'ascii-char') {
    var n = parseInt(val, 10);
    var ch = (!isNaN(n) && n > 32 && n < 127) ? String.fromCharCode(n) : '';
    return '<input type="text" maxlength="1" value="' + ch + '" style="width:3rem;text-align:center">';
  }
  if (s.type === 'number') {
    return '<input type="number" value="' + val + '" min="' + (s.min || 0) + '" max="' + (s.max || 9999) + '">';
  }
  if (s.type === 'password') {
    return '<input type="password" value="' + val + '" maxlength="' + (s.maxlength || 64) + '">';
  }
  return '<input type="text" value="' + val + '"' + (s.maxlength ? ' maxlength="' + s.maxlength + '"' : '') + '>';
}

async function doAction(btn) {
  var cmd      = btn.dataset.cmd;
  var endpoint = btn.dataset.endpoint || '/api/monitor';
  var prev = btn.textContent;
  btn.textContent = '…';
  btn.disabled = true;
  try {
    var r = await fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'cmd=' + encodeURIComponent(cmd)
    });
    showToast(r.ok ? 'Sent' : 'Failed', !r.ok);
  } catch(e) {
    showToast('Network error', true);
  }
  btn.textContent = prev;
  btn.disabled = false;
}

var _pencil = '<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M4 20 l0.5-3.5 L15 6 l3 3 L7.5 19.5 Z"/><line x1="13.5" y1="7.5" x2="16.5" y2="10.5"/></svg>';

function buildRow(s, val, hidden) {
  var hiddenAttr = hidden ? ' hidden' : '';
  if (s.type === 'action') {
    return '<div class="row"' + hiddenAttr + '>'
      + '<div class="row-label">' + s.label + '</div>'
      + '<button class="be-action" onclick="doAction(this)" data-cmd="' + s.cmd + '" data-endpoint="' + (s.endpoint || '/api/monitor') + '">'
      + (s.btnLabel || 'Send') + '</button>'
      + '</div>';
  }
  var disp = dispValue(s, val);
  var note = s.note ? '<span style="font-size:.65rem;color:var(--muted);margin-left:.3rem">' + s.note + '</span>' : '';
  if (s.readOnly) {
    return '<div class="row"' + hiddenAttr + ' data-key="' + (s.key || '') + '" data-type="' + (s.type || 'text') + '" data-fmt="' + (s.fmtFn || '') + '">'
      + '<div class="row-label">' + s.label + '</div>'
      + '<div class="rv">' + disp + '</div>'
      + '</div>';
  }
  return '<div class="row"' + hiddenAttr + ' data-key="' + (s.key || '') + '" data-type="' + (s.type || 'text') + '" data-fmt="' + (s.fmtFn || '') + '" data-restart="' + (s.restart ? '1' : '') + '">'
    + '<div class="row-label">' + s.label + '</div>'
    + '<div class="rv">' + disp + '</div>'
    + '<div class="ri" hidden><div style="display:flex;align-items:center">' + buildInput(s, val) + note + '</div></div>'
    + '<button class="be" onclick="startEdit(this)" title="Edit">' + _pencil + '</button>'
    + '<button class="bs" hidden onclick="doSave(this)">SAVE</button>'
    + '<button class="bc" hidden onclick="doCancel(this)">&#10005;</button>'
    + '</div>';
}

// --------------------------------------------------- emergency stop button ---

(function() {
  var hdr = document.querySelector('.page-header');

  // right-side group keeps the header at 3 flex children: [BACK] [TITLE] [GROUP]
  var rg = document.createElement('div');
  rg.className = 'hdr-right';

  // theme toggle
  var tt = document.createElement('button');
  tt.id = 'theme-toggle';
  tt.title = 'Toggle dark / light mode';
  tt.textContent = document.documentElement.dataset.theme === 'dark' ? '☀' : '☾';
  tt.onclick = _toggleTheme;
  rg.appendChild(tt);

  // e-stop
  var b = document.createElement('button');
  b.id = 'estop';
  b.title = 'Emergency Stop — halts all motors';
  b.innerHTML = '<span class="dot"></span>E-STOP';
  b.onclick = function() {
    fetch('/api/estop', { method: 'POST' })
      .then(function(r) { showToast(r.ok ? 'Emergency stop sent' : 'Stop failed', !r.ok); })
      .catch(function() { showToast('Stop failed', true); });
  };
  rg.appendChild(b);

  if (hdr) hdr.appendChild(rg);
  else document.body.appendChild(rg);
})();

// ------------------------------------------------------ restart banner -------

// Sticky, browser-scoped "a setting needs a reboot to apply" flag -- same
// mechanism already used for the theme preference (localStorage, not a
// server round-trip). Set by doSave() whenever a saved row has
// data-restart="1" (schema rows opt in via `restart:true`), and by any page
// with its own stateful reboot-required logic (e.g. connectivity.html's WCB
// panel). Cleared only once a real restart is confirmed (see _pollForRestart).
function _flagRestartRequired() {
  localStorage.setItem('amidala-restart-required', '1');
  _showRestartBanner();
}

function _showRestartBanner() {
  if (document.getElementById('restart-banner')) return;
  var main = document.querySelector('main');
  var b = document.createElement('div');
  b.id = 'restart-banner';
  b.className = 'restart-banner';
  b.innerHTML = '<span>A restart is required for setting changes to apply.</span>'
    + '<button onclick="_doRestart(this)">Restart Now</button>';
  // Inserted as <main>'s previous sibling, not inside it -- buildPage()
  // fully replaces main's innerHTML once its fetch resolves, which would
  // otherwise wipe out a banner injected before that completes.
  if (main) document.body.insertBefore(b, main);
  else document.body.appendChild(b);
}

// Shared by the auto-shown restart-required banner AND any standalone
// "Restart" button elsewhere in the UI (e.g. the Droid Status page) -- btn
// is required, the banner/span are optional and only used when present.
function _doRestart(btn) {
  if (!confirm('Restart the droid now?')) return;
  var banner = document.getElementById('restart-banner');
  var span = banner ? banner.querySelector('span') : null;
  btn.disabled = true;
  btn.textContent = 'Restarting…';
  if (span) span.textContent = 'Restarting…';
  fetch('/api/reboot', { method: 'POST' }).catch(function() {});
  // Fixed grace delay before polling starts -- mirrors update.html's OTA
  // restart flow, giving the device a moment to actually begin rebooting
  // before we start treating a fetch rejection as "still down" evidence.
  setTimeout(function() { _pollForRestart(btn, span); }, 3000);
}

function _pollForRestart(btn, span) {
  var attempts = 0;
  var timer = setInterval(function() {
    attempts++;
    if (attempts > 30) {
      clearInterval(timer);
      var msg = 'Still restarting — check the board, or reload manually.';
      if (span) span.textContent = msg;
      else if (btn) btn.textContent = 'Still restarting…';
      return;
    }
    fetch('/api/info', { cache: 'no-store' })
      .then(function(r) { if (!r.ok) throw new Error(); return r.json(); })
      .then(function() {
        clearInterval(timer);
        localStorage.removeItem('amidala-restart-required');
        location.reload();
      })
      .catch(function() { /* still restarting -- wait for the next tick */ });
  }, 2000);
}

(function() {
  // update.html already has its own, more detailed restart-polling flow --
  // a second generic banner there mid-flash would be confusing.
  if (location.pathname.indexOf('update') !== -1) return;
  if (localStorage.getItem('amidala-restart-required') === '1') _showRestartBanner();
})();

// --------------------------------------------------------------- footer -------

(function() {
  if (location.pathname.indexOf('monitor') !== -1) return;
  var f = document.createElement('footer');
  f.innerHTML = '<a href="https://github.com/thePunderWoman/Amidala/wiki" target="_blank" rel="noopener">DOCUMENTATION</a>';
  document.body.appendChild(f);
})();

// ------------------------------------------------- hash-based tab nav --------

function initHashTabs(defaultTab, onSwitch) {
  function activate(raw) {
    var requested = ((raw || '').replace(/^#/, ''));
    var tabs = document.querySelectorAll('.tab');
    var matched = false;
    tabs.forEach(function(el) { if (el.dataset.tab === requested) matched = true; });
    var t = matched ? requested : defaultTab;
    tabs.forEach(function(el) { el.classList.toggle('active', el.dataset.tab === t); });
    if (onSwitch) onSwitch(t);
  }
  window.addEventListener('hashchange', function() { activate(location.hash); });
  activate(location.hash);
}

function showHashTab(t) {
  location.hash = '#' + t;
}

// Tracks the current page's schema + last-known config snapshot so a saved
// edit can re-evaluate other rows' when: predicates without a full reload.
var _schema = null;
var _configData = null;

function buildPage(SCHEMA, endpoint, callback) {
  fetch(endpoint)
    .then(function(r) { return r.json(); })
    .then(function(d) {
      _schema = SCHEMA;
      _configData = d;
      var html = '';
      var sectionSkip = false;
      SCHEMA.forEach(function(s) {
        if (s.section) {
          // Section-level when: (e.g. hardware-type gating) is still
          // evaluated once at load -- unlike row-level when:, nothing in
          // this codebase edits the gating key inline from the same page,
          // so there's no live case to react to yet.
          sectionSkip = s.when ? !s.when(d) : false;
          if (!sectionSkip) html += '<div class="section-label">' + s.section + '</div>';
          return;
        }
        if (sectionSkip) return;
        // Row-level when: rows are still rendered (just hidden), so they
        // can be shown live via _applyWhenVisibility() after a save.
        var rowHidden = !!(s.when && !s.when(d));
        if (s.type === 'action') { html += buildRow(s, '', rowHidden); return; }
        var val = (d[s.key] !== undefined) ? String(d[s.key]) : '?';
        html += buildRow(s, val, rowHidden);
      });
      document.querySelector('main').innerHTML = html;
      if (callback) callback(d);
    })
    .catch(function() {
      var el = document.getElementById('status');
      if (el) el.textContent = 'Failed to load settings.';
    });
}

// Re-hides/shows every row-level when:-gated row against the current
// _configData snapshot. Called after doSave() so toggling a gating field
// (e.g. a bool) immediately shows/hides its dependent rows in place.
function _applyWhenVisibility() {
  if (!_schema || !_configData) return;
  _schema.forEach(function(s) {
    if (s.section || !s.when) return;
    var row = document.querySelector('[data-key="' + s.key + '"]');
    if (row) row.hidden = !s.when(_configData);
  });
}

// Rebuilds the <select> for every row whose schema uses a function-based
// options (see _resolveOptions / pins.html) against the live _configData
// snapshot. Each row's <select> is only ever built once at page load
// (buildRow()), so without this, saving e.g. dout1pin=40 wouldn't stop
// servo1pin's dropdown from still offering 40 until a full page reload.
function _refreshDynamicOptions() {
  if (!_schema || !_configData) return;
  _schema.forEach(function(s) {
    if (s.section || typeof s.options !== 'function') return;
    var row = document.querySelector('[data-key="' + s.key + '"]');
    var sel = row && row.querySelector('select');
    if (!sel) return;
    var val = (_configData[s.key] !== undefined) ? String(_configData[s.key]) : sel.value;
    sel.outerHTML = buildInput(s, val);
  });
}
