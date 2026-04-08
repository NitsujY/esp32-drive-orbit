import { createReadStream, existsSync } from 'node:fs';
import { readFile, stat } from 'node:fs/promises';
import { createServer } from 'node:http';
import { networkInterfaces } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { WebSocket, WebSocketServer } from 'ws';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..', '..');
const previewDir = path.join(repoRoot, 'preview');
const sourceDir = path.join(repoRoot, 'frontend', 'dashboard');

const cliArgs = process.argv.slice(2);

function readCliArg(name, fallback) {
  const prefix = `--${name}=`;
  const match = cliArgs.find((arg) => arg.startsWith(prefix));
  return match ? match.slice(prefix.length) : fallback;
}

const host = readCliArg('host', process.env.HOST ?? '0.0.0.0');
const port = Number.parseInt(readCliArg('port', process.env.PORT ?? '8123'), 10);
const wsPath = readCliArg('ws-path', process.env.WS_PATH ?? '/ws');
const proxyWsTarget = readCliArg('proxy-ws', process.env.WS_PROXY_TARGET ?? '').trim();
const hasWsProxy = proxyWsTarget.length > 0;
const activeSockets = new Set();
let shuttingDown = false;

const contentTypes = new Map([
  ['.html', 'text/html; charset=utf-8'],
  ['.css', 'text/css; charset=utf-8'],
  ['.js', 'application/javascript; charset=utf-8'],
  ['.json', 'application/json; charset=utf-8'],
  ['.svg', 'image/svg+xml'],
]);

const mounts = [
  {
    prefix: '/dashboard',
    dir: sourceDir,
    fallback: path.join(sourceDir, 'index.html'),
  },
  {
    prefix: '/',
    dir: previewDir,
    fallback: null,
  },
];

const telemetry = {
  sequence: 0,
  phase: 0,
  speed: 0,
  rpm: 760,
  coolant: 78,
  weatherTemp: 24,
  weatherCode: 1,
  battery: 12640,
  fuel: 82,
  headlights: false,
};

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function nextSnapshot() {
  const previousSpeed = telemetry.speed;

  telemetry.sequence += 1;
  telemetry.phase += 0.08;

  const wave = (Math.sin(telemetry.phase) + 1) / 2;
  const sweep = (Math.sin(telemetry.phase * 0.35) + 1) / 2;
  const cycleTicks = 480;
  const cyclePosition = telemetry.sequence % cycleTicks;
  let speedTarget = 0;

  if (cyclePosition < 60) {
    speedTarget = 0;
  } else if (cyclePosition < 150) {
    speedTarget = ((cyclePosition - 60) / 90) * 72;
  } else if (cyclePosition < 310) {
    speedTarget = 68 + wave * 52;
  } else if (cyclePosition < 400) {
    speedTarget = (1 - (cyclePosition - 310) / 90) * 92;
  }

  const speedJitter = speedTarget === 0 ? 0 : Math.sin(telemetry.phase * 2.4) * 4;
  telemetry.speed = clamp(Math.round(speedTarget + speedJitter), 0, 132);
  telemetry.rpm = telemetry.speed === 0
    ? Math.round(760 + Math.sin(telemetry.phase * 1.6) * 35)
    : Math.round(850 + telemetry.speed * 34 + Math.sin(telemetry.phase * 1.8) * 260);
  telemetry.coolant = clamp(Math.round(78 + sweep * 16), 76, 96);
  telemetry.weatherTemp = clamp(Math.round(22 + Math.sin(telemetry.phase * 0.12) * 7), 14, 33);
  telemetry.battery = clamp(Math.round(12420 + Math.cos(telemetry.phase * 1.3) * 180), 12100, 12780);
  telemetry.fuel = clamp(Math.round(82 - (telemetry.sequence / 1800) % 58), 24, 82);
  telemetry.headlights = Math.sin(telemetry.phase * 0.16) > 0.45;

  const weatherCycle = Math.floor((telemetry.sequence / 90) % 6);
  telemetry.weatherCode = [0, 1, 2, 3, 61, 95][weatherCycle];

  const efficiencyFactor = clamp(1.02 - telemetry.speed / 260, 0.72, 1.02);
  const range = Math.round(telemetry.fuel * 4.6 * efficiencyFactor);
  const acceleration = clamp((telemetry.speed - previousSpeed) * 14, -140, 140);

  return {
    v: 1,
    seq: telemetry.sequence,
    up: telemetry.sequence * 125,
    spd: telemetry.speed,
    rpm: telemetry.rpm,
    clt: telemetry.coolant,
    wt: telemetry.weatherTemp,
    wc: telemetry.weatherCode,
    bat: telemetry.battery,
    fuel: telemetry.fuel,
    rng: range,
    acc: acceleration,
    wifi: 1,
    hl: telemetry.headlights ? 1 : 0,
    obd: 1,
    fresh: 1,
  };
}

function getMount(pathname) {
  for (const mount of mounts) {
    if (mount.prefix === '/') {
      return mount;
    }

    if (pathname === mount.prefix || pathname.startsWith(`${mount.prefix}/`)) {
      return mount;
    }
  }

  return null;
}

async function resolveFile(requestUrl) {
  const url = new URL(requestUrl ?? '/', 'http://localhost');
  const decodedPath = decodeURIComponent(url.pathname);
  const mount = getMount(decodedPath);

  if (!mount) {
    return null;
  }

  if (mount.prefix !== '/' && decodedPath === mount.prefix) {
    return { redirect: `${mount.prefix}/${url.search}` };
  }

  let relativePath = '/index.html';
  if (mount.prefix === '/') {
    relativePath = decodedPath === '/' ? '/index.html' : decodedPath;
  } else if (decodedPath !== `${mount.prefix}/`) {
    relativePath = decodedPath.slice(mount.prefix.length) || '/index.html';
  }

  const absolutePath = path.join(mount.dir, relativePath);
  const resolvedPath = path.resolve(absolutePath);
  if (!resolvedPath.startsWith(mount.dir)) {
    return null;
  }

  if (!existsSync(resolvedPath)) {
    if (!mount.fallback) {
      return null;
    }

    return { filePath: mount.fallback };
  }

  const fileStat = await stat(resolvedPath);
  if (fileStat.isDirectory()) {
    if (!url.pathname.endsWith('/')) {
      return { redirect: `${url.pathname}/${url.search}` };
    }

    return { filePath: path.join(resolvedPath, 'index.html') };
  }

  return { filePath: resolvedPath };
}

const server = createServer(async (request, response) => {
  try {
    const resolved = await resolveFile(request.url ?? '/');
    if (!resolved) {
      response.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
      response.end('Not found');
      return;
    }

    if ('redirect' in resolved) {
      response.writeHead(302, { Location: resolved.redirect });
      response.end();
      return;
    }

    const { filePath } = resolved;

    const extension = path.extname(filePath);
    const contentType = contentTypes.get(extension) ?? 'application/octet-stream';

    response.writeHead(200, {
      'Cache-Control': 'no-store',
      'Content-Type': contentType,
    });
    createReadStream(filePath).pipe(response);
  } catch (error) {
    response.writeHead(500, { 'Content-Type': 'text/plain; charset=utf-8' });
    response.end(error instanceof Error ? error.message : 'Unknown server error');
  }
});

server.on('connection', (socket) => {
  activeSockets.add(socket);
  socket.on('close', () => {
    activeSockets.delete(socket);
  });
});

const websocketServer = new WebSocketServer({ server, path: wsPath });

const broadcastTimer = hasWsProxy ? null : setInterval(() => {
  const payload = JSON.stringify(nextSnapshot());
  for (const client of websocketServer.clients) {
    if (client.readyState === client.OPEN) {
      client.send(payload);
    }
  }
}, 125);

function proxyWebSocketConnection(browserSocket) {
  const upstreamSocket = new WebSocket(proxyWsTarget);
  const queuedMessages = [];

  function flushQueue() {
    while (queuedMessages.length > 0 && upstreamSocket.readyState === WebSocket.OPEN) {
      const entry = queuedMessages.shift();
      upstreamSocket.send(entry.data, { binary: entry.isBinary });
    }
  }

  browserSocket.on('message', (data, isBinary) => {
    if (!isBinary && data.toString() === 'schema') {
      void readFile(path.join(sourceDir, 'schema.json'), 'utf8').then((schema) => {
        if (browserSocket.readyState === WebSocket.OPEN) {
          browserSocket.send(schema);
        }
      });
      return;
    }

    if (upstreamSocket.readyState === WebSocket.OPEN) {
      upstreamSocket.send(data, { binary: isBinary });
      return;
    }

    if (upstreamSocket.readyState === WebSocket.CONNECTING) {
      queuedMessages.push({ data, isBinary });
    }
  });

  upstreamSocket.on('open', flushQueue);

  upstreamSocket.on('message', (data, isBinary) => {
    if (browserSocket.readyState === WebSocket.OPEN) {
      browserSocket.send(data, { binary: isBinary });
    }
  });

  upstreamSocket.on('close', () => {
    if (browserSocket.readyState === WebSocket.OPEN) {
      browserSocket.close();
    }
  });

  upstreamSocket.on('error', (error) => {
    console.warn('[dev] WebSocket proxy upstream error:', error instanceof Error ? error.message : error);
    if (browserSocket.readyState === WebSocket.OPEN) {
      browserSocket.close(1011, 'Proxy upstream error');
    }
  });

  browserSocket.on('close', () => {
    if (upstreamSocket.readyState === WebSocket.OPEN || upstreamSocket.readyState === WebSocket.CONNECTING) {
      upstreamSocket.close();
    }
  });

  browserSocket.on('error', () => {
    if (upstreamSocket.readyState === WebSocket.OPEN || upstreamSocket.readyState === WebSocket.CONNECTING) {
      upstreamSocket.terminate();
    }
  });
}

websocketServer.on('connection', async (socket) => {
  if (hasWsProxy) {
    proxyWebSocketConnection(socket);
    return;
  }

  socket.send(JSON.stringify(nextSnapshot()));

  socket.on('message', async (rawMessage) => {
    const text = rawMessage.toString();
    if (text === 'snapshot') {
      socket.send(JSON.stringify(nextSnapshot()));
      return;
    }

    if (text === 'schema') {
      const schema = await readFile(path.join(sourceDir, 'schema.json'), 'utf8');
      socket.send(schema);
    }
  });
});

server.listen(port, host, () => {
  const networkLines = [];
  for (const [name, addresses] of Object.entries(networkInterfaces())) {
    for (const address of addresses ?? []) {
      if (address.family !== 'IPv4' || address.internal) {
        continue;
      }
      networkLines.push({ name, address: address.address });
    }
  }

  console.log(`[dev] Preview root: ${previewDir}`);
  console.log(`[dev] Dashboard source: ${sourceDir}`);
  console.log(`[dev] Preview hub: http://${host}:${port}/`);
  console.log(`[dev] Live dashboard: http://${host}:${port}/dashboard/`);
  console.log(`[dev] Mock WebSocket: ws://${host === '0.0.0.0' ? '127.0.0.1' : host}:${port}${wsPath}`);
  if (hasWsProxy) {
    console.log(`[dev] WebSocket proxy: ${wsPath} -> ${proxyWsTarget}`);
  } else {
    console.log('[dev] This mode serves the preview hub plus the source dashboard with mock telemetry; use `npm run build:web` and `node tools/dashboard/release-server.mjs` for built-asset parity with ESP32.');
  }
  if (networkLines.length > 0) {
    console.log('[dev] LAN URLs:');
    for (const line of networkLines) {
      console.log(`  ${line.name}: http://${line.address}:${port}/  dashboard: http://${line.address}:${port}/dashboard/`);
    }
  }
});

for (const signal of ['SIGINT', 'SIGTERM']) {
  process.on(signal, () => {
    if (shuttingDown) {
      return;
    }

    shuttingDown = true;
    if (broadcastTimer) {
      clearInterval(broadcastTimer);
    }

    for (const client of websocketServer.clients) {
      client.terminate();
    }

    websocketServer.close();
    server.close(() => process.exit(0));

    for (const socket of activeSockets) {
      socket.destroy();
    }

    setTimeout(() => process.exit(0), 150).unref();
  });
}