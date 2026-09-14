'use strict';

const assert = require('node:assert/strict');
const childProcess = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');

const PLUGIN_ROOT = path.resolve(__dirname, '..');
const SOURCE_PATH = path.join(PLUGIN_ROOT, 'src', 'index.html');
const DIST_PATH = path.join(PLUGIN_ROOT, 'dist', 'index.html');
const BUILD_SCRIPT = path.join(PLUGIN_ROOT, 'scripts', 'build.cjs');

function runBuild(outputDirectory, checkOnly = false) {
  const args = [BUILD_SCRIPT];
  if (checkOnly) args.push('--check');
  childProcess.execFileSync(process.execPath, args, {
    cwd: PLUGIN_ROOT,
    env: { ...process.env, AGMAKER_TITLE_DIST_DIR: outputDirectory },
    stdio: 'pipe',
  });
}

test('project.title 构建可从受控源再生确定性编辑器产物', (t) => {
  const temporaryOutput = fs.mkdtempSync(path.join(os.tmpdir(), 'project-title-build-'));
  t.after(() => fs.rmSync(temporaryOutput, { recursive: true, force: true }));

  fs.writeFileSync(path.join(temporaryOutput, 'index.html'), '<!-- stale artifact -->');
  runBuild(temporaryOutput);
  assert.deepEqual(
    fs.readFileSync(path.join(temporaryOutput, 'index.html')),
    fs.readFileSync(SOURCE_PATH),
    '构建必须覆盖陈旧产物并生成完整受控源内容',
  );

  const firstBuild = fs.readFileSync(path.join(temporaryOutput, 'index.html'));
  runBuild(temporaryOutput);
  assert.deepEqual(fs.readFileSync(path.join(temporaryOutput, 'index.html')), firstBuild, '重复构建必须字节级确定');
  runBuild(temporaryOutput, true);
});

test('project.title 已交付的 dist 与受控源一致', () => {
  assert.deepEqual(fs.readFileSync(DIST_PATH), fs.readFileSync(SOURCE_PATH));
  childProcess.execFileSync(process.execPath, [BUILD_SCRIPT, '--check'], {
    cwd: PLUGIN_ROOT,
    stdio: 'pipe',
  });
});

test('project.title 删除不可用：契约声明 disabled 且无删除实现', () => {
  const source = fs.readFileSync(SOURCE_PATH, 'utf8');

  assert.ok(source.includes('"available": false, "disabled": true, "operation": "delete"'));
  assert.ok(!source.includes('delete_title'));
  assert.ok(!source.includes('function deleteTitle'));
  assert.ok(!source.includes('function performDelete'));
  assert.ok(!source.includes('onDeleteClick'));
  assert.ok(!source.includes('btnDelete'));
  assert.ok(!source.includes('btn-delete'));
  assert.ok(!source.includes('aigame-plugin-delete-approval-request'));
  assert.ok(!source.includes('X-Harness-Interactive-Approval'));
  assert.ok(!source.includes('interactiveApproval'));
  assert.ok(!source.includes('DELETE_APPROVAL_TIMEOUT_MS'));
  assert.ok(!source.includes('localStorage'));
  assert.ok(!source.includes('sessionStorage'));

  // id 字段标签与设计契约一致，避免字段契约校验失败
  assert.ok(source.includes("label.textContent = '编号'"));
});

test('project.title 已移除简称（abbreviation）字段：Schema 与编辑器均不再包含', () => {
  const source = fs.readFileSync(SOURCE_PATH, 'utf8');
  const schemaText = fs.readFileSync(path.join(PLUGIN_ROOT, 'schemas', 'title.schema.json'), 'utf8');

  assert.doesNotMatch(source, /abbreviation/i);
  assert.doesNotMatch(source, /field-abbreviation/);
  assert.doesNotMatch(schemaText, /abbreviation/i);

  const schema = JSON.parse(schemaText);
  assert.deepEqual(Object.keys(schema.properties).sort(), ['description', 'id', 'name']);
  assert.deepEqual(schema.required, ['id', 'name']);
  assert.strictEqual(schema.additionalProperties, false);
});
