function updateTopic(){
  const c = document.getElementById('consumer_id').value || '...';
  const d = document.getElementById('device_id').value || '<mac>';
  document.getElementById('derived_topic').textContent = 'tally/device/' + c + '/' + d;
}

async function load(){
  try{
    const r = await fetch('/api/config');
    const d = await r.json();
    document.getElementById('device_name').value = d.device_name || '';
    document.getElementById('led_brightness').value = d.led_brightness ?? 255;
    document.getElementById('bright-val').textContent = d.led_brightness ?? 255;
    document.getElementById('wifi_ssid').value = d.wifi_ssid || '';
    document.getElementById('mqtt_url').value = d.mqtt_url || '';
    document.getElementById('consumer_id').value = d.consumer_id || 'aedes';
    document.getElementById('device_id').value = d.device_id || '';
    document.getElementById('led_layout').value = d.led_layout || '';
    updateTopic();
  }catch(_e){
    msg('Failed to load config');
  }
}

async function scan(){
  try{
    await fetch('/api/scan/start', { method: 'POST' });
    for(let i = 0; i < 20; i++){
      const r = await fetch('/api/scan');
      const d = await r.json();
      const results = Array.isArray(d.results) ? d.results : [];
      const sel = document.getElementById('wifi_ssid_sel');
      sel.innerHTML = '<option value="">-- scan results --</option>';
      results.forEach((s) => {
        const ssid = typeof s === 'string' ? s : (s.ssid || '');
        if(!ssid) return;
        const o = document.createElement('option');
        o.value = ssid;
        o.textContent = typeof s === 'object' && s.rssi !== undefined
          ? `${ssid} (${s.rssi} dBm)`
          : ssid;
        sel.appendChild(o);
      });
      if(!d.scanning) return;
      await new Promise(resolve => setTimeout(resolve, 350));
    }
    msg('Scan timed out');
  }catch(_e){
    msg('Scan failed');
  }
}

async function saveSection(section){
  let body = {};
  switch(section){
    case 'device':
      body = {
        device_name: document.getElementById('device_name').value,
        led_brightness: parseInt(document.getElementById('led_brightness').value, 10)
      };
      break;
    case 'wifi': {
      body = { wifi_ssid: document.getElementById('wifi_ssid').value };
      const pass = document.getElementById('wifi_pass').value;
      if(pass) body.wifi_pass = pass;
      break;
    }
    case 'mqtt':
      body = { mqtt_url: document.getElementById('mqtt_url').value };
      break;
    case 'beacon':
      body = {
        consumer_id: document.getElementById('consumer_id').value,
        device_id: document.getElementById('device_id').value
      };
      break;
    case 'layout':
      body = { led_layout: document.getElementById('led_layout').value };
      break;
    default:
      return;
  }

  try{
    const r = await fetch('/api/config', {
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify(body)
    });
    if(r.ok){
      let rebootNeeded = true;
      try{
        const d = await r.json();
        rebootNeeded = d.reboot_needed === true;
      }catch(_e){
      }
      setRebootNeeded(rebootNeeded);
      msg(rebootNeeded ? 'Saved - reboot required' : 'Saved');
    }else{
      msg('Save failed');
    }
  }catch(_e){
    msg('Save failed');
  }
}

async function rebootDevice(){
  try{
    const r = await fetch('/api/reboot', { method:'POST' });
    if(r.ok){
      msg('Rebooting...');
      setRebootNeeded(false);
    }else{
      msg('Reboot failed');
    }
  }catch(_e){
    msg('Reboot failed');
  }
}

function setRebootNeeded(needed){
  document.getElementById('reboot_alert').className = needed ? 'alert show' : 'alert';
}

function statusItem(label, value, ok){
  return '<span class="status-item">' +
    '<span class="dot ' + (ok ? 'on' : 'off') + '"></span>' +
    '<span class="status-label">' + label + '</span>' +
    '<span class="status-value">' + value + '</span>' +
  '</span>';
}

async function pollStatus(){
  try{
    const r = await fetch('/api/status');
    const d = await r.json();
    document.getElementById('status').innerHTML =
      statusItem('WiFi', d.wifi ? d.ip : 'disconnected', d.wifi) +
      statusItem('MQTT', d.mqtt ? 'connected' : 'disconnected', d.mqtt) +
      statusItem('Beacon', d.beacon ? 'online' : 'offline', d.beacon);
    setRebootNeeded(d.reboot_needed === true);
  }catch(_e){
  }
}

function msg(t){
  document.getElementById('msg').textContent = t;
}

load();
scan();
pollStatus();
setInterval(pollStatus, 3000);
