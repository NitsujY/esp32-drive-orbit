import path from 'node:path';
import { buildDashboardTarget, repoRoot } from './build-target.mjs';

const outputDir = path.join(repoRoot, 'data');

await buildDashboardTarget({
  compressTextAssets: true,
  outputDir,
});

console.log(`[build] Wrote dashboard assets to ${outputDir}`);