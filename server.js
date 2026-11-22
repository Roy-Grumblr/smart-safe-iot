const express = require('express');
const mqtt = require('mqtt');
const path = require('path');

const app = express();
const port = process.env.PORT || 3000;
const dataTopic = 'smartsafe/groupX/data';
const commandTopic = 'smartsafe/groupX/cmd';
const allowedActions = new Set([
  'UNLOCK',
  'LOCK',
  'RESET_ALARM',
  'DISABLE_VIBRATION',
  'ENABLE_VIBRATION'
]);

let latestData = {
  lock: 'LOCKED',
  vibration: 'NORMAL',
  alarm: 'OFF',
  pin_errors: 0,
  vibration_sensor: 'ENABLED',
  last_event: 'Menunggu data perangkat'
};
let history = [];

const client = mqtt.connect('mqtt://broker.emqx.io:1883');

client.on('connect', () => {
  console.log('Connected to MQTT broker');
  client.subscribe(dataTopic, (error) => {
    if (error) {
      console.error(`Failed to subscribe to ${dataTopic}:`, error.message);
    } else {
      console.log(`Subscribed to ${dataTopic}`);
    }
  });
});

client.on('message', (topic, payload) => {
  if (topic !== dataTopic) return;

  try {
    const parsed = JSON.parse(payload.toString());
    if (parsed && typeof parsed === 'object' && !Array.isArray(parsed)) {
      latestData = parsed;
      history.push({
        time: new Date().toLocaleTimeString('id-ID', { hour: '2-digit', minute: '2-digit', second: '2-digit' }),
        event: latestData.last_event || 'Update',
        status: latestData.alarm === 'ON'
          ? 'Alarm'
          : (latestData.lock === 'UNLOCKED' ? 'Berhasil' : 'Info')
      });
      if (history.length > 20) history.shift();
    } else {
      console.error(`Ignoring non-object MQTT payload on ${topic}`);
    }
  } catch (error) {
    console.error(`Ignoring invalid JSON on ${topic}:`, error.message);
  }
});

client.on('error', (error) => {
  console.error('MQTT connection error:', error.message);
});

app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

app.get('/api/status', (req, res) => {
  res.json(latestData);
});

app.get('/api/history', (req, res) => {
  res.json(history);
});

app.post('/api/control', (req, res) => {
  const { action } = req.body || {};
  if (!allowedActions.has(action)) {
    return res.status(400).json({
      error: 'action must be UNLOCK, LOCK, RESET_ALARM, DISABLE_VIBRATION, or ENABLE_VIBRATION'
    });
  }

  if (!client.connected) {
    return res.status(503).json({ error: 'MQTT broker is not connected' });
  }

  client.publish(commandTopic, JSON.stringify({ action }), (error) => {
    if (error) {
      console.error(`Failed to publish to ${commandTopic}:`, error.message);
      return res.status(502).json({ error: 'Failed to send command to device' });
    }

    res.json({ ok: true, action });
  });
});

const server = app.listen(port, () => {
  console.log(`Smart Safe Dashboard running at http://localhost:${port}`);
});

function shutdown() {
  server.close(() => {
    client.end(false, () => process.exit(0));
  });
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
