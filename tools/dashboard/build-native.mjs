import path from 'node:path';
import { buildDashboardTarget, repoRoot } from './build-target.mjs';

const outputDir = path.join(repoRoot, 'build', 'capacitor');

await buildDashboardTarget({
  compressTextAssets: false,
  outputDir,
});

console.log(`[build:native] Wrote dashboard assets to ${outputDir}`);