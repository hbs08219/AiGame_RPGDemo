'use strict';
(() => {
  const CONFIG_ID = 'agmaker-data-resource-editor-shell';
  const AUTH_MESSAGE = 'aigame-auth-token';
  const AUTH_REQUEST = 'aigame-auth-token-request';
  const configElement = document.getElementById(CONFIG_ID);
  if (!configElement) throw new Error(`缺少 ${CONFIG_ID} 配置`);
  const config = JSON.parse(configElement.textContent || '{}');
  if (config.shellVersion !== 1 || !Array.isArray(config.resources) || config.resources.length === 0) throw new Error('DataResource Editor Shell 配置无效');
  const createActionPlacement = config.createActionPlacement || 'list-header';
  if (!['list-header', 'detail-actions'].includes(createActionPlacement)) throw new Error('DataResource Editor Shell createActionPlacement 无效');

  let authToken = '';
  let authReceived = false;
  const authWaiters = [];
  let selectedResourceId = config.resources[0].id;
  let selectedRevision = '';
  let items = [];
  let baseline = '';
  let dirty = false;
  const root = document.getElementById('agmaker-editor-root');
  if (!root) throw new Error('缺少 agmaker-editor-root bootstrap 节点');

  const style = document.createElement('style');
  style.textContent = ':root{color-scheme:dark;font-family:Inter,system-ui,"Microsoft YaHei",sans-serif;background:#0b1120;color:#eef3ff}*{box-sizing:border-box}body{margin:0;min-width:320px}header{height:56px;padding:0 20px;display:flex;align-items:center;gap:16px;justify-content:space-between;border-bottom:1px solid #263653;background:#111827}h1{font-size:18px;margin:0}.resource-switch{min-width:160px;padding:8px;border:1px solid #40506d;border-radius:6px;background:#0d1320;color:#fff}.shell{display:grid;grid-template-columns:minmax(240px,32%) minmax(0,1fr);height:calc(100vh - 56px)}aside{border-right:1px solid #263653;padding:14px;overflow:auto}.list-header{display:flex;align-items:center;gap:8px}.search{width:100%;min-width:0;padding:9px 11px;border:1px solid #40506d;border-radius:7px;background:#0b1322;color:#fff}.list{display:grid;gap:6px;margin-top:10px}.record{width:100%;padding:9px;text-align:left;border:1px solid transparent;border-radius:7px;background:#151f31;color:#cdd8eb;cursor:pointer}.record[aria-current="true"]{border-color:#806ee6;background:#292449}.detail{padding:22px;overflow:auto}form{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:13px}label{display:grid;gap:6px;color:#b9c7de;font-size:13px}input,textarea,select{padding:9px;border:1px solid #40506d;border-radius:6px;background:#0d1320;color:#fff}textarea{min-height:96px;resize:vertical}.actions{display:flex;gap:8px;flex-wrap:wrap;margin:16px 0}button{padding:8px 14px;border:1px solid #485a78;border-radius:6px;background:#22304a;color:#eef3ff;cursor:pointer}button:disabled{opacity:.45;cursor:not-allowed}.danger{border-color:#864956;background:#40202a}.status{min-height:44px;padding:10px;border:1px solid #263653;border-radius:7px;background:#0d1320;color:#aebcd2;white-space:pre-wrap}.empty{padding:24px 8px;color:#7f8da5;text-align:center}@media(max-width:720px){.shell{grid-template-columns:1fr;height:auto}aside{border-right:0;border-bottom:1px solid #263653}form{grid-template-columns:1fr}}';
  document.head.append(style);
  const experienceStyle = document.createElement('style');
  experienceStyle.textContent = '.title-group{min-width:0}.title-group h1{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.resource-context{display:block;margin-top:2px;color:#94a6c3;font-size:12px}.header-actions{display:flex;align-items:center;gap:8px}.list-toolbar{display:flex;align-items:center;justify-content:space-between;gap:8px;margin-bottom:10px}.list-title{display:block;color:#e5edff;font-size:13px}.record-count{display:block;margin-top:2px;color:#7f91ae;font-size:11px}.record{display:grid;gap:3px;min-height:52px}.record-title{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;color:#edf3ff;font-weight:600}.record-meta{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;color:#8fa1bd;font-size:11px}.detail-heading{display:flex;align-items:baseline;gap:8px;margin:0 0 16px}.detail-heading h2{margin:0;font-size:16px}.detail-hint{color:#7f91ae;font-size:12px}@media(max-width:720px){.header-actions{width:100%}.resource-switch{flex:1}.list-toolbar{margin-top:2px}}';
  document.head.append(experienceStyle);
  const createControl = '<button type="button" data-crud-operation="create">新建</button>';
  const listCreateControl = createActionPlacement === 'list-header' ? createControl : '';
  const detailCreateControl = createActionPlacement === 'detail-actions' ? createControl : '';
  root.innerHTML = `<header><div class="title-group"><h1></h1><span id="resource-context" class="resource-context"></span></div><div class="header-actions"><select id="resource-switch" class="resource-switch" aria-label="数据资源"></select><button type="button" data-crud-operation="read">刷新</button></div></header><main class="shell"><aside><div class="list-toolbar"><div><strong id="list-title" class="list-title"></strong><span id="record-count" class="record-count"></span></div><div class="list-header"><input id="search" class="search" type="search" placeholder="搜索…" aria-label="搜索">${listCreateControl}</div></div><div id="records" class="list"><div class="empty">正在加载…</div></div></aside><section class="detail" aria-label="详情编辑"><div class="detail-heading"><h2 id="detail-title"></h2><span class="detail-hint">字段会按数据合同校验并保存</span></div><form id="editor-form"></form><div class="actions">${detailCreateControl}<button type="button" data-crud-operation="update">保存修改</button><button class="danger" type="button" data-crud-operation="delete">删除</button></div><div id="output" class="status" role="status" aria-live="polite">尚未选择记录，可填写表单后新建。</div></section></main>`;

  const title = root.querySelector('h1');
  const resourceSwitch = root.querySelector('#resource-switch');
  const form = root.querySelector('#editor-form');
  const output = root.querySelector('#output');
  const records = root.querySelector('#records');
  const search = root.querySelector('#search');
  const resourceContext = root.querySelector('#resource-context');
  const listTitle = root.querySelector('#list-title');
  const recordCount = root.querySelector('#record-count');
  title.textContent = config.displayName || config.pluginId;
  for (const resource of config.resources) {
    const option = document.createElement('option');
    option.value = resource.id;
    option.textContent = resource.displayName || resource.id;
    resourceSwitch.append(option);
  }

  function currentResource() {
    return config.resources.find((resource) => resource.id === selectedResourceId) || config.resources[0];
  }

  function projectIdFromUrl() {
    const projectId = new URLSearchParams(window.location.search).get('projectId');
    return typeof projectId === 'string' ? projectId.trim() : '';
  }

  function askParentForAuth() {
    try {
      if (window.parent && window.parent !== window) window.parent.postMessage({ type: AUTH_REQUEST }, '*');
    } catch {}
  }

  window.addEventListener('message', (event) => {
    if (!event.data || event.data.type !== AUTH_MESSAGE) return;
    authReceived = true;
    authToken = typeof event.data.token === 'string' ? event.data.token : '';
    while (authWaiters.length) authWaiters.shift()();
  });

  function waitForParentAuth() {
    if (authReceived || !window.parent || window.parent === window) return Promise.resolve();
    askParentForAuth();
    return new Promise((resolve) => {
      const timer = setTimeout(resolve, 500);
      authWaiters.push(() => { clearTimeout(timer); resolve(); });
    });
  }

  askParentForAuth();
  setTimeout(askParentForAuth, 150);
  setTimeout(askParentForAuth, 500);
  setTimeout(askParentForAuth, 1200);

  async function invoke(action, args) {
    if (!action || action.available !== true) throw new Error('此操作未在 PluginSpec 中启用');
    const projectId = projectIdFromUrl();
    if (!projectId) throw new Error('URL 缺少 projectId，拒绝访问项目数据');
    const url = new URL(action.path, window.location.href);
    url.searchParams.set('projectId', projectId);
    await waitForParentAuth();
    if (!authToken) throw new Error('未收到父窗口鉴权令牌，拒绝访问项目数据');
    const headers = {
      'Content-Type': 'application/json',
      'X-Aigame-Project-Context': 'iframe',
      'X-Aigame-Project-Id': projectId,
      Authorization: `Bearer ${authToken}`,
    };
    const response = await fetch(url.toString(), { method: action.method, credentials: 'include', headers, body: JSON.stringify(args) });
    let payload;
    try { payload = await response.json(); } catch { throw new Error(`HTTP ${response.status}`); }
    if (!response.ok || payload?.ok === false || payload?.error) {
      const error = new Error(typeof payload?.error === 'string' ? payload.error : payload?.error?.message || `HTTP ${response.status}`);
      error.status = response.status;
      error.code = payload?.code || payload?.error?.code || null;
      error.details = payload?.details || payload?.error?.details || {};
      throw error;
    }
    return payload?.data ?? payload;
  }

  function fieldNames(resource) {
    return Object.keys(resource.schema?.properties || {});
  }

  function uiFor(resource, name) {
    return resource.fieldUi?.[name] || {};
  }

  function listVisibleFields(resource) {
    return fieldNames(resource).filter((name) => uiFor(resource, name).listVisible === true);
  }

  function updateResourceChrome(resource, shownCount = items.length) {
    const displayName = resource.displayName || resource.id;
    resourceContext.textContent = `${displayName} · 共享数据编辑器`;
    listTitle.textContent = `${displayName}列表`;
    recordCount.textContent = `共 ${items.length} 条${shownCount !== items.length ? ` · 显示 ${shownCount} 条` : ''}`;
  }

  function renderForm() {
    const resource = currentResource();
    updateResourceChrome(resource);
    form.replaceChildren();
    for (const name of fieldNames(resource)) {
      const schema = resource.schema.properties[name] || {};
      const ui = uiFor(resource, name);
      const label = document.createElement('label');
      label.htmlFor = `field-${resource.id}-${name}`;
      label.append(document.createTextNode(`${ui.label || name}${resource.required?.includes(name) ? ' *' : ''}`));
      let input;
      if (ui.control === 'textarea') input = document.createElement('textarea');
      else {
        input = document.createElement('input');
        input.type = ['number', 'integer'].includes(schema.type) ? 'number' : schema.type === 'boolean' ? 'checkbox' : 'text';
      }
      input.id = label.htmlFor;
      input.name = name;
      input.required = resource.required?.includes(name) === true;
      input.readOnly = ui.control === 'readonly' || ui.generated === true;
      if (ui.help) {
        const help = document.createElement('small');
        help.textContent = ui.help;
        label.append(input, help);
      } else label.append(input);
      form.append(label);
    }
    updateButtons();
    fill(null);
  }

  function updateButtons() {
    const actions = currentResource().actions || {};
    for (const control of root.querySelectorAll('[data-crud-operation]')) {
      const operation = control.dataset.crudOperation;
      control.disabled = actions[operation]?.available !== true;
    }
  }

  function valueOf(input, type) {
    if (type === 'boolean') return input.checked;
    if (type === 'number' || type === 'integer') return input.value === '' ? '' : Number(input.value);
    return input.value;
  }

  function documentValue() {
    const resource = currentResource();
    const value = {};
    for (const name of fieldNames(resource)) {
      const input = form.elements[name];
      value[name] = valueOf(input, resource.schema.properties[name]?.type);
      if (uiFor(resource, name).generated && !value[name]) {
        value[name] = `GEN_${Date.now().toString(36).toUpperCase()}`;
        input.value = value[name];
      }
    }
    return value;
  }

  function formSnapshot() {
    const resource = currentResource();
    return JSON.stringify(Object.fromEntries(fieldNames(resource).map((name) => {
      const input = form.elements[name];
      return [name, input?.type === 'checkbox' ? input.checked : input?.value ?? ''];
    })));
  }

  function markClean() { baseline = formSnapshot(); dirty = false; }
  function updateDirty() { dirty = formSnapshot() !== baseline; }
  function allowDiscard() {
    if (!dirty || config.interactions?.dirtyGuard === false) return true;
    return window.confirm('当前有未保存修改，确认放弃吗？');
  }

  function itemDocument(item) {
    return item?.document && typeof item.document === 'object' ? item.document : item;
  }

  function itemRevision(item) {
    return item?.revision || item?.etag || item?.document?.revision || item?.document?._revision || '';
  }

  function fill(item) {
    const resource = currentResource();
    const record = item ? itemDocument(item) : null;
    for (const name of fieldNames(resource)) {
      const input = form.elements[name];
      const value = record?.[name];
      if (input.type === 'checkbox') input.checked = value === true;
      else input.value = value ?? '';
    }
    selectedRevision = item ? itemRevision(item) : '';
    const id = record?.[resource.idField];
    root.querySelector('#detail-title').textContent = record ? `编辑 ${id || '记录'}` : `新建${resource.displayName || resource.id}`;
    markClean();
  }

  function renderList() {
    const resource = currentResource();
    const query = search.value.trim().toLocaleLowerCase();
    const shown = items.filter((item) => !query || JSON.stringify(itemDocument(item)).toLocaleLowerCase().includes(query));
    updateResourceChrome(resource, shown.length);
    records.replaceChildren();
    if (!shown.length) {
      const empty = document.createElement('div');
      empty.className = 'empty';
      empty.textContent = items.length ? '没有匹配记录' : '暂无数据，可在左侧新建';
      records.append(empty);
      return;
    }
    const visibleFields = listVisibleFields(resource);
    for (const item of shown) {
      const record = itemDocument(item);
      const recordId = String(record?.[resource.idField] || item?.id || '未命名记录');
      const primaryField = visibleFields.find((name) => name !== resource.idField && record?.[name] !== undefined) || resource.idField;
      const primaryText = String(record?.[primaryField] || recordId);
      const secondaryFields = visibleFields.filter((name) => name !== primaryField && record?.[name] !== undefined).slice(0, 2);
      const control = document.createElement('button');
      control.type = 'button';
      control.className = 'record';
      const recordTitle = document.createElement('span');
      recordTitle.className = 'record-title';
      recordTitle.textContent = primaryText;
      const recordMeta = document.createElement('span');
      recordMeta.className = 'record-meta';
      recordMeta.textContent = [primaryField === resource.idField ? '' : recordId, ...secondaryFields.map((name) => String(record[name]))].filter(Boolean).join(' · ') || '未命名记录';
      control.append(recordTitle, recordMeta);
      control.setAttribute('aria-current', String(form.elements[resource.idField]?.value === String(record?.[resource.idField] || '')));
      control.addEventListener('click', () => {
        if (!allowDiscard()) return;
        fill(item);
        renderList();
        output.textContent = '已选择记录';
      });
      records.append(control);
    }
  }

  async function showRevisionConflict(resource) {
    const localDocument = documentValue();
    const currentId = localDocument[resource.idField];
    output.replaceChildren(document.createTextNode('版本冲突：远端记录已更新。'));
    const reload = document.createElement('button');
    reload.type = 'button';
    reload.dataset.conflictAction = 'reload';
    reload.textContent = '重新加载远端';
    reload.addEventListener('click', async () => {
      try {
        const envelope = await invoke(resource.actions.read, { query: '', limit: 200 });
        const result = envelope?.result ?? envelope;
        items = Array.isArray(result) ? result : Array.isArray(result?.items) ? result.items : [];
        const remote = items.find((item) => String(itemDocument(item)?.[resource.idField]) === String(currentId));
        if (remote) fill(remote);
        renderList();
        output.textContent = remote ? '已重新加载远端版本，请检查后重试保存' : '远端记录已不存在，请刷新列表';
      } catch (reloadError) { output.textContent = `重新加载失败：${reloadError?.message || String(reloadError)}`; }
    });
    output.append(document.createTextNode(' '), reload);
  }

  async function run(operation) {
    const resource = currentResource();
    output.textContent = `正在${({ read: '加载', create: '新建', update: '保存', delete: '删除' })[operation] || '处理'}…`;
    try {
      let envelope;
      if (operation === 'read') envelope = await invoke(resource.actions.read, { query: search.value, limit: 200 });
      else if (operation === 'create') envelope = await invoke(resource.actions.create, { document: documentValue() });
      else if (operation === 'update') {
        const document = documentValue();
        const id = document[resource.idField];
        const patch = { ...document };
        delete patch[resource.idField];
        envelope = await invoke(resource.actions.update, { id, patch, ...(selectedRevision ? { revision: selectedRevision } : {}) });
      } else {
        const id = documentValue()[resource.idField];
        if (!id) throw new Error('请先选择要删除的记录');
        if (!window.confirm(`确认删除 ${id}？此操作不可撤销。`)) {
          output.textContent = '已取消删除';
          return;
        }
        envelope = await invoke(resource.actions.delete, { id, ...(selectedRevision ? { revision: selectedRevision } : {}) });
      }
      const result = envelope?.result ?? envelope;
      selectedRevision = result?.revision || result?.etag || selectedRevision;
      if (operation === 'read') {
        items = Array.isArray(result) ? result : Array.isArray(result?.items) ? result.items : [];
        renderList();
        output.textContent = items.length ? `已加载 ${items.length} 条记录` : '暂无数据，可在右侧新建';
      } else {
        output.textContent = operation === 'delete' ? '删除成功' : '保存成功';
        if (operation === 'delete') fill(null);
        else markClean();
        await run('read');
      }
    } catch (error) {
      if (operation === 'update' && (error?.status === 409 || error?.code === 'REVISION_CONFLICT' || /revision|版本|冲突/i.test(error?.message || ''))) await showRevisionConflict(resource);
      else output.textContent = `操作失败：${error?.message || String(error)}`;
    }
  }

  resourceSwitch.addEventListener('change', () => {
    const nextResourceId = resourceSwitch.value;
    if (!allowDiscard()) { resourceSwitch.value = selectedResourceId; return; }
    selectedResourceId = nextResourceId;
    selectedRevision = '';
    items = [];
    search.value = '';
    renderForm();
    run('read');
  });
  for (const control of root.querySelectorAll('[data-crud-operation]')) control.addEventListener('click', () => {
    if (control.dataset.crudOperation === 'read' && !allowDiscard()) return;
    run(control.dataset.crudOperation);
  });
  form.addEventListener('input', updateDirty);
  window.addEventListener('beforeunload', (event) => { if (dirty && config.interactions?.dirtyGuard !== false) { event.preventDefault(); event.returnValue = ''; } });
  search.addEventListener('input', renderList);
  renderForm();
  run('read');
})();
