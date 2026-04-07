import { mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { gzipSync } from 'node:zlib';
import { transform } from 'esbuild';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

export const repoRoot = path.resolve(__dirname, '..', '..');

const sourceDir = path.join(repoRoot, 'frontend', 'dashboard');

function minifyHtml(source) {
  return source
    .replace(/>\s+</g, '><')
    .replace(/\s{2,}/g, ' ')
    .replace(/\n/g, ' ')
    .trim();
}

async function writeTextAsset(outputDir, fileName, content, compressTextAssets) {
  const filePath = path.join(outputDir, fileName);
  await writeFile(filePath, content);

  if (compressTextAssets) {
    await writeFile(`${filePath}.gz`, gzipSync(content, { level: 9 }));
  }
}

async function writeBinaryAsset(outputDir, fileName, content) {
  const filePath = path.join(outputDir, fileName);
  await writeFile(filePath, content);
}

export async function buildDashboardTarget({ outputDir, compressTextAssets }) {
  await rm(outputDir, { recursive: true, force: true });
  await mkdir(outputDir, { recursive: true });

  const [htmlSource, cssSource, jsSource, schemaSource, iconSource,
    maskableIconSource, appleTouchIconSource, icon192Source, icon512Source] = await Promise.all([
    readFile(path.join(sourceDir, 'index.html'), 'utf8'),
    readFile(path.join(sourceDir, 'styles.css'), 'utf8'),
    readFile(path.join(sourceDir, 'app.js'), 'utf8'),
    readFile(path.join(sourceDir, 'schema.json'), 'utf8'),
    readFile(path.join(sourceDir, 'icon.svg'), 'utf8'),
    readFile(path.join(sourceDir, 'icon-maskable.svg'), 'utf8'),
    readFile(path.join(sourceDir, 'apple-touch-icon.png')),
    readFile(path.join(sourceDir, 'icon-192.png')),
    readFile(path.join(sourceDir, 'icon-512.png')),
  ]);

  const cssResult = await transform(cssSource, {
    loader: 'css',
    minify: true,
    target: 'es2020',
  });

  const jsResult = await transform(jsSource, {
    loader: 'js',
    minify: true,
    target: 'es2020',
  });

  await Promise.all([
    writeTextAsset(outputDir, 'index.html', minifyHtml(htmlSource), compressTextAssets),
    writeTextAsset(outputDir, 'styles.css', cssResult.code, compressTextAssets),
    writeTextAsset(outputDir, 'app.js', jsResult.code, compressTextAssets),
    writeTextAsset(outputDir, 'schema.json', schemaSource.trim(), compressTextAssets),
    writeTextAsset(outputDir, 'icon.svg', iconSource.trim(), compressTextAssets),
    writeTextAsset(outputDir, 'icon-maskable.svg', maskableIconSource.trim(), compressTextAssets),
    writeBinaryAsset(outputDir, 'apple-touch-icon.png', appleTouchIconSource),
    writeBinaryAsset(outputDir, 'icon-192.png', icon192Source),
    writeBinaryAsset(outputDir, 'icon-512.png', icon512Source),
  ]);
}