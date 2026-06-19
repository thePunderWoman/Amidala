/* Amidala web UI — edit-in-place widget.
   Embed script inlines this into config sub-pages.
   In dev mode (scripts/web_dev.py) it's served as /assets/edit.js. */

function startEdit(btn) {
  var row = btn.closest('.row');
  row.querySelector('.rv').hidden = true;
  row.querySelector('.ri').hidden = false;
  btn.hidden = true;
  row.querySelector('.bs').hidden = false;
  row.querySelector('.bc').hidden = false;
}

function doCancel(btn) {
  var row = btn.closest('.row');
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
      dv.textContent = row.dataset.type === 'bool' ? (val === 'y' ? 'On' : 'Off') : val;
      doCancel(row.querySelector('.bc'));
    } else {
      alert('Save failed: ' + await r.text());
    }
  } catch(e) {
    alert('Network error');
  }
  btn.textContent = prev;
  btn.disabled = false;
}
