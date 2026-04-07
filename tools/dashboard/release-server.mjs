import { createReadStream, existsSync } from 'node:fs';
import { stat } from 'node:fs/promises';
import { createServer } from 'node:http';
import { networkInterfaces } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { WebSocket, WebSocketServer } from 'ws';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..', '..');
const dataDir = path.join(repoRoot, 'data');

const cliArgs = process.argv.slice(2);

function readCliArg(name, fallback) {
  const prefix = `--${name}=`;
  const match = cliArgs.find((arg) => arg.startsWith(prefix));
  return match ? match.slice(prefix.length) : fallback;
}

const host = readCliArg('host', process.env.HOST ?? '0.0.0.0');
const port = Number.parseInt(readCliArg('port', process.env.PORT ?? '8124'), 10);
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
  ['.png', 'image/png'],
  ['.svg', 'image/svg+xml'],
  ['.webmanifest', 'application/manifest+json; charset=utf-8'],
]);

function requestAcceptsGzip(request) {
  const headerValue = request.headers['accept-encoding'];
  return typeof headerValue === 'string' && headerValue.includes('gzip');
}

async function resolveAsset(requestPath, allowGzip) {
  const normalizedPath = requestPath === '/' ? '/index.html' : requestPath;
  const decodedPath = decodeURIComponent(normalizedPath.split('?')[0]);
  const rawPath = path.join(dataDir, decodedPath);
  const resolvedPath = path.resolve(rawPath);
  if (!resolvedPath.startsWith(dataDir)) {
    return null;
  }

  if (!existsSync(resolvedPath)) {
    return path.join(dataDir, 'index.html.gz');
  }

  const fileStat = await stat(resolvedPath);
  if (fileStat.isDirectory()) {
    return path.join(resolvedPath, allowGzip ? 'index.html.gz' : 'index.html');
  }

  const gzipCandidate = `${resolvedPath}.gz`;
  if (allowGzip && !resolvedPath.endsWith('.gz') && existsSync(gzipCandidate)) {
    return gzipCandidate;
  }

  return resolvedPath;
}

const server = createServer(async (request, response) => {
  try {
    const filePath = await resolveAsset(request.url ?? '/', requestAcceptsGzip(request));
    if (!filePath) {
      response.writeHead(403, { 'Content-Type': 'text/plain; charset=utf-8' });
      response.end('Forbidden');
      return;
    }

    const isGzip = filePath.endsWith('.gz');
    const contentExtension = isGzip ? path.extname(filePath.slice(0, -3)) : path.extname(filePath);
    const contentType = contentTypes.get(contentExtension) ?? 'application/octet-stream';
    response.writeHead(200, {
      'Cache-Control': 'no-store',
      'Content-Type': contentType,
      'Content-Disposition': 'inline',
      'Vary': 'Accept-Encoding',
      ...(isGzip ? { 'Content-Encoding': 'gzip' } : {}),
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

const websocketServer = hasWsProxy ? new WebSocketServer({ server, path: wsPath }) : null;

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
    console.warn('[release-preview] WebSocket proxy upstream error:', error instanceof Error ? error.message : error);
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

if (websocketServer) {
  websocketServer.on('connection', proxyWebSocketConnection);
}

server.listen(port, host, () => {
  const networkLines = [];
  for (const [name, addresses] of Object.entries(networkInterfaces())) {
    for (const address of addresses ?? []) {
      if (address.family !== 'IPv4' || address.internal) {
        continue;
      }
      networkLines.push(`  ${name}: http://${address.address}:${port}/`);
    }
  }

  console.log(`[release-preview] Serving built assets from ${dataDir}`);
  console.log(`[release-preview] Local URL: http://${host}:${port}/`);
  if (hasWsProxy) {
    console.log(`[release-preview] WebSocket proxy: ${wsPath} -> ${proxyWsTarget}`);
  }
  if (networkLines.length > 0) {
    console.log('[release-preview] LAN URLs:');
    for (const line of networkLines) {
      console.log(line);
    }
  }
});

for (const signal of ['SIGINT', 'SIGTERM']) {
  process.on(signal, () => {
    if (shuttingDown) {
      return;
    }

    shuttingDown = true;
    for (const client of websocketServer?.clients ?? []) {
      client.terminate();
    }

    websocketServer?.close();
    server.close(() => process.exit(0));

    for (const socket of activeSockets) {
      socket.destroy();
    }

    setTimeout(() => process.exit(0), 150).unref();
  });
}