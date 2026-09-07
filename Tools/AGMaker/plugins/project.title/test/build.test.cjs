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

test('project.title 删除审批桥只把一次性令牌用于本次删除请求', () => {
  const source = fs.readFileSync(SOURCE_PATH, 'utf8');

  assert.match(source, /type: 'aigame-plugin-delete-approval-request'/);
  assert.match(source, /pluginId: 'project\.title'/);
  assert.match(source, /actionName: 'project\.title\.delete_title'/);
  assert.match(source, /input: \{ id: target\.id, revision: target\.revision \}/);
  assert.match(source, /data\.type !== 'aigame-plugin-delete-approval-result' \|\| data\.requestId !== requestId/);
  assert.match(source, /data\.ok === true && typeof data\.token === 'string' && data\.token/);
  assert.match(source, /result\['X-Harness-Interactive-Approval'\] = interactiveApproval/);
  assert.match(source, /headers\(interactiveApproval\)/);
  assert.match(source, /DELETE_APPROVAL_TIMEOUT_MS = 30000/);
  assert.doesNotMatch(source, /localStorage|sessionStorage/);
});
