/* Amidala web UI — edit-in-place widget + shared config page helpers.
   Embed script inlines this into every config sub-page.
   In dev mode (scripts/web_dev.py) it's served as /assets/edit.js. */

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
  btn.textContent = '...';
  btn.disabled = true;
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
      } else if (rt === 'password') {
        dv.textContent = '••••••••';
      } else {
        dv.textContent = val;
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

function dispValue(s, val) {
  if (s.type === 'bool') return val === 'y' ? 'On' : 'Off';
  if (s.type === 'select') {
    var found = (s.options || []).find(function(op) { return op.v === String(val); });
    return found ? found.l : val;
  }
  if (s.type === 'password') return '••••••••';
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

function buildRow(s, val) {
  if (s.type === 'action') {
    return '<div class="row">'
      + '<div class="row-label">' + s.label + '</div>'
      + '<button class="be" onclick="doAction(this)" data-cmd="' + s.cmd + '" data-endpoint="' + (s.endpoint || '/api/monitor') + '">'
      + (s.btnLabel || 'Send') + '</button>'
      + '</div>';
  }
  var disp = dispValue(s, val);
  var note = s.note ? '<span style="font-size:.65rem;color:var(--dim);margin-left:.3rem">' + s.note + '</span>' : '';
  if (s.readOnly) {
    return '<div class="row" data-key="' + (s.key || '') + '" data-type="' + (s.type || 'text') + '">'
      + '<div class="row-label">' + s.label + '</div>'
      + '<div class="rv">' + disp + '</div>'
      + '</div>';
  }
  return '<div class="row" data-key="' + (s.key || '') + '" data-type="' + (s.type || 'text') + '">'
    + '<div class="row-label">' + s.label + '</div>'
    + '<div class="rv">' + disp + '</div>'
    + '<div class="ri" hidden><div style="display:flex;align-items:center">' + buildInput(s, val) + note + '</div></div>'
    + '<button class="be" onclick="startEdit(this)" title="Edit">&#9998;</button>'
    + '<button class="bs" hidden onclick="doSave(this)" title="Save">&#10003;</button>'
    + '<button class="bc" hidden onclick="doCancel(this)" title="Cancel">&#10005;</button>'
    + '</div>';
}

// --------------------------------------------------- emergency stop button ---

(function() {
  var b = document.createElement('button');
  b.id = 'estop';
  b.textContent = 'E-Stop';
  b.title = 'Emergency Stop — halts all motors';
  b.onclick = function() {
    fetch('/api/estop', { method: 'POST' })
      .then(function(r) { showToast(r.ok ? 'Emergency stop sent' : 'Stop failed', !r.ok); })
      .catch(function() { showToast('Stop failed', true); });
  };
  document.body.appendChild(b);
})();

// ------------------------------------------------- hash-based tab nav --------
// initHashTabs(defaultTab, onSwitch)
//   Reads location.hash to pick the active tab on load, then listens for
//   hashchange (browser back/forward) and re-activates accordingly.
//   Expects .tab elements with data-tab="<id>" attributes on the page.
//   onSwitch(tabId) is called whenever the active tab changes.
//
// showHashTab(t)
//   Call from tab button onclick. Pushes a history entry then lets the
//   hashchange handler do the actual switch (single code path for all sources).

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
