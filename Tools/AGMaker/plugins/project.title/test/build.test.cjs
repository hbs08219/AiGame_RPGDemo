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

test('project.title 删除为编辑器内单次确认直接删除，不请求审批、不携带审批令牌', () => {
  const source = fs.readFileSync(SOURCE_PATH, 'utf8');

  assert.match(source, /function deleteTitle\(id, revision\)/);
  assert.match(source, /project\.title\.delete_title/);
  assert.match(source, /performDelete\(target\)/);
  assert.match(source, /"actionName": "project\.title\.delete_title"/);

  assert.doesNotMatch(source, /aigame-plugin-delete-approval-request/);
  assert.doesNotMatch(source, /aigame-plugin-delete-approval-result/);
  assert.doesNotMatch(source, /X-Harness-Interactive-Approval/);
  assert.doesNotMatch(source, /interactiveApproval/);
  assert.doesNotMatch(source, /DELETE_APPROVAL_TIMEOUT_MS/);
  assert.doesNotMatch(source, /localStorage|sessionStorage/);
  assert.doesNotMatch(source, /当前版本未开放删除操作/);
  assert.doesNotMatch(source, /btnDelete\.disabled = true/);

  // id 字段标签与设计契约一致，避免字段契约校验失败
  assert.match(source, /label\.textContent = '编号'/);
});
