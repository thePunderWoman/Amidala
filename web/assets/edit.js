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

function doCancel(btn) {
  var row = btn.closest('.row');
  var inp = row.querySelector('input,select');
  if (inp && inp.dataset.orig !== undefined) inp.value = inp.dataset.orig;
  row.querySelector('.rv').hidden = false;
  row.querySelector('.ri').hidden = true;
  row.querySelector('.be').hidden = false;
  row.querySelector('.bs').hidden = true;
  btn.hidden = true;
}

async function doSave(btn) {
  var row = btn.closest('.row');
  var key = row.dataset.key;
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
      var rt = row.dataset.type;
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
      doCancel(row.querySelector('.bc'));
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

function dispValue(s, val) {
  if (s.type === 'bool') return val === 'y' ? 'On' : 'Off';
  if (s.type === 'select') {
    var found = (s.options || []).find(function(op) { return op.v === String(val); });
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
    var opts = (s.options || []).map(function(op) {
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

function buildRow(s, val) {
  if (s.type === 'action') {
    return '<div class="row">'
      + '<div class="row-label">' + s.label + '</div>'
      + '<button class="be-action" onclick="doAction(this)" data-cmd="' + s.cmd + '" data-endpoint="' + (s.endpoint || '/api/monitor') + '">'
      + (s.btnLabel || 'Send') + '</button>'
      + '</div>';
  }
  var disp = dispValue(s, val);
  var note = s.note ? '<span style="font-size:.65rem;color:var(--muted);margin-left:.3rem">' + s.note + '</span>' : '';
  if (s.readOnly) {
    return '<div class="row" data-key="' + (s.key || '') + '" data-type="' + (s.type || 'text') + '" data-fmt="' + (s.fmtFn || '') + '">'
      + '<div class="row-label">' + s.label + '</div>'
      + '<div class="rv">' + disp + '</div>'
      + '</div>';
  }
  return '<div class="row" data-key="' + (s.key || '') + '" data-type="' + (s.type || 'text') + '" data-fmt="' + (s.fmtFn || '') + '">'
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

function buildPage(SCHEMA, endpoint, callback) {
  fetch(endpoint)
    .then(function(r) { return r.json(); })
    .then(function(d) {
      var html = '';
      var skip = false;
      SCHEMA.forEach(function(s) {
        if (s.section) {
          skip = s.when ? !s.when(d) : false;
          if (!skip) html += '<div class="section-label">' + s.section + '</div>';
          return;
        }
        if (skip) return;
        if (s.when && !s.when(d)) return;
        if (s.type === 'action') { html += buildRow(s, ''); return; }
        var val = (d[s.key] !== undefined) ? String(d[s.key]) : '?';
        html += buildRow(s, val);
      });
      document.querySelector('main').innerHTML = html;
      if (callback) callback(d);
    })
    .catch(function() {
      var el = document.getElementById('status');
      if (el) el.textContent = 'Failed to load settings.';
    });
}
