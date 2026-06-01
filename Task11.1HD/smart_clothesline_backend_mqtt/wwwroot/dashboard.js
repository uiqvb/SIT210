async function sendCommand(command) {
  const res = await fetch('/api/command', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({command})
  });

  const data = await res.json();

  if (!data.ok) {
    alert(data.error || 'Command failed');
  }

  await refresh();
}

function text(id, value) {
  const el = document.getElementById(id);
  if (el) el.textContent = value ?? '--';
}

function yesNo(value) {
  if (value === true) return 'YES';
  if (value === false) return 'NO';
  return '--';
}

async function refresh() {
  const res = await fetch('/api/status');
  const data = await res.json();

  const latest = data.latest || {};
  const weather = data.weather || {};
  const fallback = data.fallback || {};
  const mqtt = data.mqtt || {};

  const ds = data.data_status || '--';
  const el = document.getElementById('data_status');
  el.textContent = ds;
  el.className = 'status-' + ds.toLowerCase();

  text('state', latest.state);
  text('target', latest.target);
  text('event', latest.event);
  text('line_pattern', latest.line_pattern);
  text('age', data.last_update_age_seconds == null ? '--' : data.last_update_age_seconds + ' s');

  text('mqtt_connected', yesNo(mqtt.connected));
  text('mqtt_broker', mqtt.broker ? `${mqtt.broker}:${mqtt.port}` : '--');
  text('mqtt_telemetry_topic', mqtt.telemetry_topic);
  text('mqtt_command_topic', mqtt.command_topic);
  text('mqtt_last_message', mqtt.last_message_time);
  text('mqtt_error', mqtt.last_error);
  text('last_command_published', data.last_command_published || mqtt.last_command_published);

  text('rain_raw', latest.rain_raw);
  text('rain_status', latest.rain_status);
  text('left_lux', latest.left_lux);
  text('right_lux', latest.right_lux);
  text('light_mode', latest.light_mode);
  text('line_lost_fault', yesNo(latest.line_lost_fault));
  text('movement_timeout_fault', yesNo(latest.movement_timeout_fault));
  text('light_tracking_timeout_fault', yesNo(latest.light_tracking_timeout_fault));
  text('recommendation', data.recommendation);
  text('laundry_active', data.laundry_active ? 'YES' : 'NO');
  text('pending_command', data.pending_command);

  text('weather_location', weather.location || '--');
  text('weather_source', weather.source || 'Open-Meteo');
  text('weather_rain_likely', yesNo(weather.rain_likely));
  text('weather_probability', weather.max_precip_probability_next_hours == null ? '--' : weather.max_precip_probability_next_hours + '%');
  text('weather_precip', weather.total_precip_next_hours_mm == null ? '--' : weather.total_precip_next_hours_mm + ' mm');
  text('fallback_reason', fallback.reason);
  text('fallback_command', fallback.recommended_command);

  const eventsBody = document.getElementById('events_body');
  eventsBody.innerHTML = '';

  (data.events || []).slice(0, 20).forEach(e => {
    const extra = [];

    if (e.source) extra.push('source=' + e.source);
    if (e.line_pattern) extra.push('line=' + e.line_pattern);
    if (e.rain_status) extra.push('rain=' + e.rain_status);
    if (e.reason) extra.push('reason=' + e.reason);
    if (e.broker) extra.push('broker=' + e.broker);

    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${e.timestamp || ''}</td><td class="mono">${e.event || ''}</td><td>${e.state || ''}</td><td>${e.target || ''}</td><td>${extra.join(', ')}</td>`;
    eventsBody.appendChild(tr);
  });

  await refreshLogs();
}

async function refreshLogs() {
  const res = await fetch('/api/logs?limit=20');
  const logs = await res.json();
  const body = document.getElementById('logs_body');
  body.innerHTML = '';

  logs.forEach(r => {
    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${r.timestamp || ''}</td><td>${r.source || ''}</td><td>${r.state || ''}</td><td>${r.target || ''}</td><td class="mono">${r.event || ''}</td><td>${r.rain_status || ''} / ${r.rain_raw || ''}</td><td class="mono">${r.line_pattern || ''}</td><td>${r.command_sent || ''} (${r.command_source || ''})</td><td>${r.fallback_reason || ''}</td>`;
    body.appendChild(tr);
  });
}


async function loadWeatherConfig() {
  const res = await fetch('/api/weather/config');
  const cfg = await res.json();
  const mapping = {
    weather_location_name: cfg.location_name,
    weather_latitude: cfg.latitude,
    weather_longitude: cfg.longitude,
    weather_timezone: cfg.timezone
  };

  Object.entries(mapping).forEach(([id, value]) => {
    const el = document.getElementById(id);
    if (el && document.activeElement !== el) {
      el.value = value ?? '';
    }
  });
}

async function saveWeatherConfig() {
  const payload = {
    location_name: document.getElementById('weather_location_name').value,
    latitude: document.getElementById('weather_latitude').value,
    longitude: document.getElementById('weather_longitude').value,
    timezone: document.getElementById('weather_timezone').value
  };

  const res = await fetch('/api/weather/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(payload)
  });

  const data = await res.json();

  if (!data.ok) {
    alert(data.error || 'Could not save weather location');
    return;
  }

  await loadWeatherConfig();
  await refresh();
}

async function refreshWeather() {
  const res = await fetch('/api/weather/refresh', { method: 'POST' });
  const data = await res.json();

  if (!data.ok) {
    alert('Weather refresh failed');
  }

  await refresh();
}

async function checkFallback() {
  const res = await fetch('/api/fallback');
  const fallback = await res.json();
  text('fallback_reason', fallback.reason);
  text('fallback_command', fallback.recommended_command);
}

loadWeatherConfig();
refresh();
setInterval(refresh, 1000);
