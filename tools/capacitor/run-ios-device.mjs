import { mkdtemp, readFile, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..', '..');
const projectPath = path.join(repoRoot, 'ios', 'App', 'App.xcodeproj');
const derivedDataPath = path.join(repoRoot, 'build', 'ios', 'device-run');
const bundleIdentifier = 'com.driveorbit.dashboard';

function parseArgs(argv) {
  const parsed = {};

  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === '--device' || arg === '--udid') {
      parsed.device = argv[index + 1];
      index += 1;
    }
  }

  return parsed;
}

function run(command, args, options = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, {
      cwd: repoRoot,
      env: process.env,
      stdio: 'inherit',
      ...options,
    });

    child.on('exit', (code) => {
      if (code === 0) {
        resolve();
        return;
      }

      reject(new Error(`${command} ${args.join(' ')} failed with exit code ${code}`));
    });
  });
}

async function getIosDevices() {
  const tempDir = await mkdtemp(path.join(os.tmpdir(), 'drive-orbit-ios-'));
  const jsonPath = path.join(tempDir, 'devices.json');

  try {
    await run('xcrun', ['devicectl', 'list', 'devices', '--json-output', jsonPath], {
      stdio: ['inherit', 'inherit', 'inherit'],
    });

    const payload = JSON.parse(await readFile(jsonPath, 'utf8'));
    const devices = payload.result?.devices ?? [];

    return devices.filter((device) => (
      device.hardwareProperties?.platform === 'iOS' &&
      device.hardwareProperties?.reality === 'physical' &&
      device.connectionProperties?.pairingState === 'paired'
    ));
  } finally {
    await rm(tempDir, { recursive: true, force: true });
  }
}

async function resolveDevice() {
  const args = parseArgs(process.argv.slice(2));
  const requested = args.device ?? process.env.IOS_DEVICE_UDID ?? process.env.IOS_DEVICE_NAME;
  const devices = await getIosDevices();

  if (devices.length === 0) {
    throw new Error('No paired physical iPhone was found. Connect and unlock your phone, then try again.');
  }

  if (requested) {
    const matched = devices.find((device) => (
      device.hardwareProperties?.udid === requested ||
      device.identifier === requested ||
      device.deviceProperties?.name === requested
    ));

    if (matched) {
      return matched;
    }

    throw new Error(`Requested device '${requested}' was not found among paired iOS devices.`);
  }

  if (devices.length === 1) {
    return devices[0];
  }

  const names = devices.map((device) => `${device.deviceProperties?.name} (${device.hardwareProperties?.udid})`).join(', ');
  throw new Error(`Multiple paired iOS devices found. Re-run with --device <udid|name>. Devices: ${names}`);
}

async function main() {
  const device = await resolveDevice();
  const udid = device.hardwareProperties?.udid;
  const displayName = device.deviceProperties?.name ?? udid;
  const builtAppPath = path.join(derivedDataPath, 'Build', 'Products', 'Debug-iphoneos', 'App.app');

  console.log(`\n[cap:run:ios] Target device: ${displayName} (${udid})`);
  await run('node', ['tools/dashboard/build-native.mjs']);
  await run('npx', ['cap', 'sync', 'ios']);
  await run('xcodebuild', [
    '-project', projectPath,
    '-scheme', 'App',
    '-configuration', 'Debug',
    '-destination', `id=${udid}`,
    '-derivedDataPath', derivedDataPath,
    '-allowProvisioningUpdates',
    'build',
  ]);
  await run('xcrun', ['devicectl', 'device', 'install', 'app', '--device', udid, builtAppPath]);
  await run('xcrun', ['devicectl', 'device', 'process', 'launch', '--device', udid, '--terminate-existing', bundleIdentifier]);
}

main().catch((error) => {
  console.error(`\n[cap:run:ios] ${error.message}`);
  process.exit(1);
});