const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const root = path.resolve(__dirname, '..');

global.window = {
  LTR: {
    clientLog() {},
    goToLogin() {},
    escapeHtml(s) { return String(s); },
    iconFor() { return 'file'; },
    formatBytes(n) { return String(n); },
    supportsFolderPick() { return true; },
  },
};
global.navigator = { storage: null };
global.TextEncoder = TextEncoder;

function load(rel) {
  const code = fs.readFileSync(path.join(root, rel), 'utf8');
  vm.runInThisContext(code, { filename: rel });
}

load('assets/web/js/upload.js');
load('assets/web/js/p2p_session.js');

const files = [
  { name: 'b.jpg', size: 20, lastModified: 2, webkitRelativePath: 'Photos/sub/b.jpg' },
  { name: 'a.jpg', size: 10, lastModified: 1, webkitRelativePath: 'Photos/a.jpg' },
];
const bundles = window.LTR.uploadTest.groupFolderFiles(files);
assert.strictEqual(bundles.length, 1);
assert.strictEqual(bundles[0].kind, 'folder');
assert.strictEqual(bundles[0].rootName, 'Photos');
assert.strictEqual(bundles[0].totalSize, 30);
assert.deepStrictEqual(
  bundles[0].files.map((f) => f.webkitRelativePath),
  ['Photos/a.jpg', 'Photos/sub/b.jpg']);

const state = {
  sessionId: 'sid',
  files,
  totalBytes: 30,
  currentFileIdx: 1,
  bundle: { kind: 'folder', name: 'Photos', fileCount: 2 },
};
const sessionMeta = window.LTR.p2pSessionTest.buildSessionMeta(state);
assert.strictEqual(sessionMeta.bundleKind, 'folder');
assert.strictEqual(sessionMeta.bundleName, 'Photos');
assert.strictEqual(sessionMeta.bundleFileCount, 2);

const fileMeta = window.LTR.p2pSessionTest.buildFileMeta(state, files[1]);
assert.strictEqual(fileMeta.kind, 'file-meta');
assert.strictEqual(fileMeta.relativePath, 'Photos/a.jpg');
assert.strictEqual(fileMeta.fileId, 'sid:1');

assert.strictEqual(
  window.LTR.p2pSessionTest.zipPath('/Photos/../safe/a.jpg'),
  'Photos/safe/a.jpg');

console.log('test_web_folder_logic OK');
