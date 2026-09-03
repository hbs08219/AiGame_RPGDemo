'use strict';

const fs = require('node:fs');
const path = require('node:path');

const PLUGIN_ROOT = path.resolve(__dirname, '..');
const SOURCE_PATH = path.join(PLUGIN_ROOT, 'src', 'index.html');
const outputDirectory = process.env.AGMAKER_TITLE_DIST_DIR
  ? path.resolve(process.env.AGMAKER_TITLE_DIST_DIR)
  : path.join(PLUGIN_ROOT, 'dist');
const OUTPUT_PATH = path.join(outputDirectory, 'index.html');
const checkOnly = process.argv.slice(2).includes('--check');

function fail(message) {
  process.stderr.write(`project.title build: ${message}\n`);
  process.exitCode = 1;
}

if (!fs.existsSync(SOURCE_PATH)) {
  fail(`缺少受控源文件 ${path.relative(PLUGIN_ROOT, SOURCE_PATH)}`);
} else {
  const source = fs.readFileSync(SOURCE_PATH);

  if (checkOnly) {
    if (!fs.existsSync(OUTPUT_PATH) || !fs.readFileSync(OUTPUT_PATH).equals(source)) {
      fail('dist/index.html 与 src/index.html 不一致；请运行 npm run build。');
    }
  } else {
    fs.mkdirSync(outputDirectory, { recursive: true });
    if (!fs.existsSync(OUTPUT_PATH) || !fs.readFileSync(OUTPUT_PATH).equals(source)) {
      fs.writeFileSync(OUTPUT_PATH, source);
    }
    process.stdout.write(`project.title build: wrote ${path.relative(PLUGIN_ROOT, OUTPUT_PATH)}\n`);
  }
}
