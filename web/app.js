/* JSON-RPC 2.0 client */
let _rpcId = 1;
async function rpc(method, params) {
    const res = await fetch('/rpc', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ jsonrpc: '2.0', method, params: params !== undefined ? params : null, id: _rpcId++ })
    });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const data = await res.json();
    if (data.error) throw new Error(data.error.message + ' (' + data.error.code + ')');
    return data.result;
}

/* Toast */
let _toastTimer;
function toast(msg, isErr) {
    const el = document.getElementById('toast');
    el.textContent = msg;
    el.className = isErr ? 'err' : 'ok';
    el.style.display = 'block';
    clearTimeout(_toastTimer);
    _toastTimer = setTimeout(() => { el.style.display = 'none'; }, 3000);
}

/* Render flat/nested object as <dt><dd> pairs */
function renderDl(el, obj, prefix) {
    el.innerHTML = '';
    function walk(o, pre) {
        for (const [k, v] of Object.entries(o)) {
            const label = pre ? pre + '.' + k : k;
            if (v !== null && typeof v === 'object' && !Array.isArray(v)) {
                walk(v, label);
            } else {
                const dt = document.createElement('dt');
                dt.textContent = label;
                const dd = document.createElement('dd');
                dd.textContent = (v === null || v === undefined) ? '-' : String(v);
                el.appendChild(dt);
                el.appendChild(dd);
            }
        }
    }
    walk(obj, prefix || '');
}

/* Tab switching */
document.querySelectorAll('.tab').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelectorAll('.tab').forEach(b => b.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(s => s.classList.remove('active'));
        btn.classList.add('active');
        document.getElementById(btn.dataset.tab).classList.add('active');
    });
});

/* ─── DASHBOARD ─── */
async function refreshDashboard() {
    try {
        const [sys, ht, wifi] = await Promise.all([rpc('Sys.Info'), rpc('Ht.Info'), rpc('Wifi.Info')]);

        document.getElementById('hdr-name').textContent = sys.device_name || 'ESP32-C3';
        document.getElementById('status-dot').className = 'dot online';

        const sysEl = document.getElementById('sys-info');
        renderDl(sysEl, {
            model: sys.model,
            idf: sys.idf_version,
            heap: sys.free_heap + ' B',
            cores: sys.cores,
            rev: sys.revision
        });

        const htEl = document.getElementById('ht-info');
        if (ht.status === 'no_data') {
            htEl.innerHTML = '<dt>status</dt><dd>no data yet</dd>';
        } else {
            renderDl(htEl, {
                temperature: ht.temperature + ' °' + (ht.unit_temp || 'C'),
                humidity: ht.humidity + ' ' + (ht.unit_humidity || '%')
            });
        }

        renderDl(document.getElementById('wifi-dash'), wifi);
    } catch (e) {
        document.getElementById('status-dot').className = 'dot offline';
        toast('Offline: ' + e.message, true);
    }
}

/* ─── WIFI ─── */
document.querySelector('[data-tab="wifi"]').addEventListener('click', async () => {
    try {
        const cfg = await rpc('Config.Get');
        const sf = document.getElementById('wifi-sta-form');
        sf.ssid.value = cfg.wifi_sta_ssid || '';
        sf.enable.checked = !!cfg.wifi_sta_enabled;
        const af = document.getElementById('wifi-ap-form');
        af.ssid.value = cfg.wifi_ap_ssid || '';
        af.enable.checked = !!cfg.wifi_ap_enabled;
    } catch (_) { }
});

document.getElementById('wifi-sta-form').addEventListener('submit', async e => {
    e.preventDefault();
    const f = e.target;
    const params = { enable: f.enable.checked };
    if (f.ssid.value) params.ssid = f.ssid.value;
    if (f.password.value) params.password = f.password.value;
    try { await rpc('Wifi.Sta.Set', params); toast('STA config applied'); }
    catch (e) { toast(e.message, true); }
});

document.getElementById('wifi-ap-form').addEventListener('submit', async e => {
    e.preventDefault();
    const f = e.target;
    const params = { enable: f.enable.checked };
    if (f.ssid.value) params.ssid = f.ssid.value;
    if (f.password.value) params.password = f.password.value;
    try { await rpc('Wifi.Ap.Set', params); toast('AP config applied'); }
    catch (e) { toast(e.message, true); }
});

/* ─── LIGHTS ─── */
async function setLed(gpio, state) {
    try {
        await rpc('Light.Led.Set', { gpio, state });
        toast('GPIO ' + gpio + ' → ' + (state ? 'ON' : 'OFF'));
    } catch (e) { toast(e.message, true); }
}

function updateRgbPreview() {
    const hex = document.getElementById('rgb-picker').value;
    const on = document.getElementById('rgb-on').checked;
    document.getElementById('rgb-preview').style.background = on ? hex : '#111';
}
document.getElementById('rgb-picker').addEventListener('input', updateRgbPreview);
document.getElementById('rgb-on').addEventListener('change', updateRgbPreview);

async function setRgb() {
    const hex = document.getElementById('rgb-picker').value;
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    const on = document.getElementById('rgb-on').checked;
    try { await rpc('Light.Rgb.Set', { r, g, b, on }); toast('RGB set'); }
    catch (e) { toast(e.message, true); }
}

/* ─── IR ─── */
document.getElementById('ir-form').addEventListener('submit', async e => {
    e.preventDefault();
    const f = e.target;
    const addr = parseInt(f.addr.value, 10);
    const cmd = parseInt(f.cmd.value, 10);
    try {
        await rpc('Ir.Send', { addr, cmd });
        document.getElementById('ir-result').textContent = `Sent addr=0x${addr.toString(16).padStart(2, '0').toUpperCase()} cmd=0x${cmd.toString(16).padStart(2, '0').toUpperCase()}`;
        toast('IR sent');
    } catch (e) {
        document.getElementById('ir-result').textContent = '';
        toast(e.message, true);
    }
});

document.getElementById('ir-record-form').addEventListener('submit', async e => {
    e.preventDefault();
    const f = e.target;
    const name = f.name.value.trim();
    const timeout_ms = parseInt(f.timeout_ms.value, 10);
    const resultEl = document.getElementById('ir-record-result');
    resultEl.textContent = 'Waiting for IR signal…';
    const btn = f.querySelector('button[type="submit"]');
    btn.disabled = true;
    try {
        const res = await rpc('Ir.Record', { name, timeout_ms });
        resultEl.textContent = `Recorded "${res.name}" addr=0x${res.addr.toString(16).padStart(2, '0').toUpperCase()} cmd=0x${res.cmd.toString(16).padStart(2, '0').toUpperCase()}`;
        toast('IR code recorded');
        irRefreshList();
    } catch (err) {
        resultEl.textContent = '';
        toast(err.message, true);
    } finally {
        btn.disabled = false;
    }
});

async function irRefreshList() {
    const el = document.getElementById('ir-list');
    el.textContent = 'Loading…';
    try {
        const list = await rpc('Ir.List', null);
        if (!list || list.length === 0) {
            el.textContent = 'No saved IR codes.';
            return;
        }
        const table = document.createElement('table');
        table.style.width = '100%';
        const header = table.insertRow();
        ['Name', 'Addr', 'Cmd'].forEach(h => { const th = document.createElement('th'); th.textContent = h; header.appendChild(th); });
        list.forEach(item => {
            const row = table.insertRow();
            row.insertCell().textContent = item.name;
            row.insertCell().textContent = '0x' + item.addr.toString(16).padStart(2, '0').toUpperCase();
            row.insertCell().textContent = '0x' + item.cmd.toString(16).padStart(2, '0').toUpperCase();
        });
        el.innerHTML = '';
        el.appendChild(table);
    } catch (err) {
        el.textContent = '';
        toast(err.message, true);
    }
}

/* ─── DISPLAY ─── */
async function setEffect(random) {
    const params = random ? null : { n: parseInt(document.getElementById('effect-n').value, 10) };
    try {
        const res = await rpc('Display.Effect', params);
        toast('Effect: ' + res.effect);
        if (random) document.getElementById('effect-n').value = res.effect;
    } catch (e) { toast(e.message, true); }
}

/* ─── MQTT ─── */
document.querySelector('[data-tab="mqtt"]').addEventListener('click', async () => {
    try {
        const [info, cfg] = await Promise.all([rpc('Mqtt.Info'), rpc('Config.Get')]);
        const el = document.getElementById('mqtt-status');
        el.innerHTML = '';
        const tag = document.createElement('span');
        tag.className = 'tag ' + (info.connected ? 'green' : 'red');
        tag.textContent = info.connected ? 'Connected' : 'Disconnected';
        el.appendChild(tag);
        renderDl(document.getElementById('mqtt-info'), {
            host: info.host,
            port: info.port,
            username: info.username
        });
        const f = document.getElementById('mqtt-form');
        f.host.value = cfg.mqtt_broker_host || '';
        f.port.value = cfg.mqtt_broker_port || 1883;
        f.username.value = cfg.mqtt_username || '';
    } catch (_) { }
});

document.getElementById('mqtt-form').addEventListener('submit', async e => {
    e.preventDefault();
    const f = e.target;
    const params = {};
    if (f.host.value) params.host = f.host.value;
    const port = parseInt(f.port.value, 10);
    if (port) params.port = port;
    if (f.username.value) params.username = f.username.value;
    if (f.password.value) params.password = f.password.value;
    try { await rpc('Mqtt.Set', params); toast('MQTT saved'); }
    catch (e) { toast(e.message, true); }
});

/* ─── CRON ─── */
document.querySelector('[data-tab="cron"]').addEventListener('click', loadCronList);

async function loadCronList() {
    try {
        const jobs = await rpc('Cron.List');
        const el = document.getElementById('cron-list');
        if (!jobs || jobs.length === 0) {
            el.innerHTML = '<p class="muted">No cron jobs.</p>';
            return;
        }
        el.innerHTML = jobs.map(j => `
      <div class="cron-job${j.enabled ? '' : ' disabled'}" data-id="${j.id}">
        <span class="cron-id">#${j.id}</span>
        <code>${escHtml(j.expression)}</code>
        <code>${escHtml(j.method)}</code>
        <button class="sm warn" onclick="cronToggle(${j.id},${j.enabled})">${j.enabled ? 'Disable' : 'Enable'}</button>
        <button class="sm danger" onclick="cronDelete(${j.id})">Del</button>
      </div>`).join('');
    } catch (e) { toast(e.message, true); }
}

function escHtml(s) {
    return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

async function cronDelete(id) {
    if (!confirm('Delete cron job #' + id + '?')) return;
    try { await rpc('Cron.Delete', { id }); toast('Job deleted'); loadCronList(); }
    catch (e) { toast(e.message, true); }
}

async function cronToggle(id, enabled) {
    try { await rpc('Cron.Update', { id, enabled: !enabled }); loadCronList(); }
    catch (e) { toast(e.message, true); }
}

document.getElementById('cron-form').addEventListener('submit', async e => {
    e.preventDefault();
    const f = e.target;
    const params = {
        expression: f.expression.value,
        method: f.method.value,
        enabled: f.enabled.checked
    };
    if (f.params.value.trim()) {
        try { params.params = JSON.parse(f.params.value); }
        catch (_) { toast('Invalid params JSON', true); return; }
    }
    try {
        await rpc('Cron.Create', params);
        toast('Cron job created');
        f.reset();
        loadCronList();
    } catch (e) { toast(e.message, true); }
});

/* ─── SYSTEM ─── */
document.querySelector('[data-tab="system"]').addEventListener('click', async () => {
    try {
        const cfg = await rpc('Config.Get');
        document.getElementById('cfg-device-name').value = cfg.device_name || '';
    } catch (_) { }
});

document.getElementById('config-form').addEventListener('submit', async e => {
    e.preventDefault();
    try {
        await rpc('Config.Set', { device_name: document.getElementById('cfg-device-name').value });
        toast('Config saved');
    } catch (e) { toast(e.message, true); }
});

document.getElementById('ota-form').addEventListener('submit', async e => {
    e.preventDefault();
    const url = e.target.url.value.trim();
    if (!url) return;
    try {
        const res = await rpc('Sys.Ota', { url });
        toast('OTA: ' + res.status);
    } catch (e) { toast(e.message, true); }
});

document.getElementById('reboot-btn').addEventListener('click', async () => {
    if (!confirm('Reboot device?')) return;
    try { await rpc('Sys.Reboot'); toast('Rebooting…'); }
    catch (e) { toast(e.message, true); }
});

document.getElementById('factory-btn').addEventListener('click', async () => {
    if (!confirm('Factory reset? All settings will be erased.')) return;
    try { await rpc('Sys.Factory'); toast('Factory reset done. Rebooting…'); }
    catch (e) { toast(e.message, true); }
});

/* ─── Init ─── */
refreshDashboard();
setInterval(refreshDashboard, 8000);
