const editor = document.getElementById('configEditor');
const validationBox = document.getElementById('validationBox');
const adapterStatus = document.getElementById('adapterStatus');
const simStatus = document.getElementById('simStatus');
const webStatus = document.getElementById('webStatus');
const adapterSummary = document.getElementById('adapterSummary');
const simSummary = document.getElementById('simSummary');
const webSummary = document.getElementById('webSummary');
const adapterBadge = document.getElementById('adapterBadge');
const simBadge = document.getElementById('simBadge');
const webBadge = document.getElementById('webBadge');
const topologyStatus = document.getElementById('topologyStatus');
const topologyMessage = document.getElementById('topologyMessage');
const topologyPreview = document.getElementById('topologyPreview');
const mapYamlUploadInput = document.getElementById('mapYamlUploadInput');
const mapPgmUploadInput = document.getElementById('mapPgmUploadInput');
const connectionBox = document.getElementById('connectionBox');
const businessMessage = document.getElementById('businessMessage');
const businessPointTable = document.getElementById('businessPointTable');
const editMessage = document.getElementById('editMessage');
const editPointTable = document.getElementById('editPointTable');
const editPathTable = document.getElementById('editPathTable');
const editPointCount = document.getElementById('editPointCount');
const editPathCount = document.getElementById('editPathCount');
let currentConfig = {};
let lastTopologyResult = {};
let lastBusinessPoints = {};
let editableTopology = { points: [], paths: [], mappingFile: '', plantJsonFile: '' };
let suppressConfigDirty = false;
let quickFormDirty = false;
let editorDirty = false;

const quickFields = [
  ['quickMqttUri', 'mqtt_uri'],
  ['quickSourceTopic', 'source_topic'],
  ['quickCommandTopic', 'command_topic'],
  ['quickOpenTcsUrl', 'open_tcs_base_url'],
  ['quickRcsUrl', 'rcs_base_url'],
  ['quickVehicleName', 'vehicle_name'],
  ['quickAgvId', 'agv_id'],
  ['quickTolerance', 'distance_tolerance_m'],
  ['quickPublishEvent', 'publish_rcs_event'],
  ['quickPointMap', 'point_id_map'],
];

const simplifyPresets = {
  detailed: {
    maxNodes: 80,
    sampleSpacingPx: 40,
    clusterRadiusPx: 7,
    minNodeDistancePx: 12,
    maxSegments: 60,
    houghThreshold: 14,
    houghMinLinePx: 24,
    houghMergeAngleDeg: 4,
    houghMergeDistancePx: 6,
    houghMergeGapPx: 35,
    loopClosureEdges: 30,
    loopClosureDistancePx: 140,
    collinearAngleDeg: 5,
  },
  balanced: {
    maxNodes: 40,
    sampleSpacingPx: 80,
    clusterRadiusPx: 12,
    minNodeDistancePx: 24,
    maxSegments: 28,
    houghThreshold: 22,
    houghMinLinePx: 45,
    houghMergeAngleDeg: 6,
    houghMergeDistancePx: 10,
    houghMergeGapPx: 70,
    loopClosureEdges: 22,
    loopClosureDistancePx: 160,
    collinearAngleDeg: 8,
  },
  simple: {
    maxNodes: 24,
    sampleSpacingPx: 120,
    clusterRadiusPx: 18,
    minNodeDistancePx: 40,
    maxSegments: 18,
    houghThreshold: 22,
    houghMinLinePx: 45,
    houghMergeAngleDeg: 8,
    houghMergeDistancePx: 14,
    houghMergeGapPx: 100,
    loopClosureEdges: 18,
    loopClosureDistancePx: 210,
    collinearAngleDeg: 11,
  },
  'very-simple': {
    maxNodes: 16,
    sampleSpacingPx: 180,
    clusterRadiusPx: 30,
    minNodeDistancePx: 70,
    maxSegments: 14,
    houghThreshold: 22,
    houghMinLinePx: 55,
    houghMergeAngleDeg: 10,
    houghMergeDistancePx: 18,
    houghMergeGapPx: 140,
    loopClosureEdges: 20,
    loopClosureDistancePx: 280,
    collinearAngleDeg: 15,
  },
};

async function loadConfig(options = {}) {
  const force = options.force === true;
  const response = await fetch('/api/config');
  let data;
  try {
    data = await response.json();
  } catch (error) {
    data = { ok: false, error: `config service returned non-JSON, HTTP ${response.status}` };
  }
  currentConfig = data.config || {};
  renderValidation(data.validation);
  if (force || (!quickFormDirty && !editorDirty)) {
    setConfigForm(currentConfig, true);
  } else {
   validationBox.textContent = '正在编辑，已保留当前表单内容；如需丢弃修改，请点击“丢弃修改，从 JSON 刷新”。\n\n' + validationBox.textContent;
    validationBox.style.color = '#2563eb';
  }
}

async function loadStatus() {
  if (!adapterStatus && !adapterSummary && !simStatus && !webStatus) {
    return;
  }
  const response = await fetch('/api/status');
  const data = await response.json();
  const adapter = data.adapter || {};
  const simulator = data.simulator || {};
  const web = data.web || {};
  renderStatusSummary(adapterSummary, adapterBadge, adapter, [
    ['Connection', adapter.connected === true ? 'MQTT connected' : (adapter.running ? 'Running, MQTT unconfirmed' : 'Stopped')],
    ['MQTT', adapter.mqttUri || '-'],
    ['反馈 topic', adapter.sourceTopic || '-'],
    ['控制 topic', adapter.commandTopic || '-'],
    ['openTCS', adapter.openTcsBaseUrl || '-'],
    ['RCS', adapter.rcsBaseUrl || '-'],
    ['车辆', adapter.vehicleName || '-'],
    ['Points', adapter.points ?? '-'],
    ['处理/更新/忽略', `${adapter.processed ?? 0} / ${adapter.updated ?? 0} / ${adapter.ignored ?? 0}`],
    ['Last update', formatStatusTime((adapter.lastUpdate || {}).time || adapter.lastReloadAt || adapter.startedAt)],
    ['异常', adapter.runtimeConfigError || adapter.lastError || '-'],
  ]);
  renderStatusSummary(simSummary, simBadge, simulator, [
    ['Connection', simulator.connected === true ? 'MQTT connected' : (simulator.running ? 'Running, MQTT unconfirmed' : 'Stopped')],
    ['MQTT', simulator.mqttUri || '-'],
    ['监听 topic', simulator.commandTopic || '-'],
    ['反馈 topic', simulator.sourceTopic || '-'],
    ['收到/发布', `${simulator.receivedCount ?? 0} / ${simulator.publishedCount ?? 0}`],
    ['Last command', formatStatusTime(simulator.lastCommandAt)],
    ['Last feedback', formatStatusTime(simulator.lastPublishedAt)],
    ['提示', simulator.lastIgnoredReason || '-'],
  ]);
  renderStatusSummary(webSummary, webBadge, web, [
    ['地址', `${web.host || '127.0.0.1'}:${web.port || 8091}`],
    ['配置文件', compactPath(web.runtimeConfigFile)],
    ['Last action', web.lastAction || '-'],
    ['Root directory', compactPath(web.rootDir)],
  ]);
  adapterStatus.textContent = JSON.stringify(adapter, null, 2);
  simStatus.textContent = JSON.stringify(simulator, null, 2);
  webStatus.textContent = JSON.stringify(web, null, 2);
}

function renderStatusSummary(container, badge, status, rows) {
  if (!container || !badge) return;
  const running = status && status.running === true;
  const hasError = Boolean(status && (status.lastError || status.runtimeConfigError));
  badge.textContent = hasError ? "Error" : (running ? "Running" : "Stopped");
  badge.className = `status-badge ${hasError ? 'is-error' : (running ? 'is-ok' : 'is-muted')}`;
  container.innerHTML = '';
  rows.forEach(([label, value]) => {
    const dt = document.createElement('dt');
    const dd = document.createElement('dd');
    dt.textContent = label;
    dd.textContent = normalizeStatusValue(value);
    if (label === '异常' && value && value !== '-') {
      dd.classList.add('status-error-text');
    }
    container.appendChild(dt);
    container.appendChild(dd);
  });
}

function normalizeStatusValue(value) {
  if (value === undefined || value === null || value === '') return '-';
  return String(value);
}

function formatStatusTime(value) {
  if (!value) return '-';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return String(value);
  return date.toLocaleString();
}

function compactPath(value) {
  if (!value) return '-';
  const text = String(value);
  const parts = text.split(/[\\/]/).filter(Boolean);
  if (parts.length <= 3) return text;
  return `${parts[0]}\\...\\${parts.slice(-2).join('\\')}`;
}

async function loadTopology() {
  const response = await fetch('/api/topology');
  const data = await response.json();
  lastTopologyResult = data || {};
  topologyStatus.textContent = JSON.stringify(data || {}, null, 2);
  if (data && data.previewFile) {
    topologyPreview.src = `/api/topology/preview?file=${encodeURIComponent(data.previewFile)}&t=${Date.now()}`;
    topologyPreview.style.display = 'block';
  }
  await loadBusinessPoints(false);
}

async function saveConfig() {
  let parsed;
  try {
    parsed = JSON.parse(editor.value);
  } catch (error) {
    validationBox.textContent = `JSON 解析失败: ${error}`;
    validationBox.style.color = '#b91c1c';
    return;
  }
  await postConfig(parsed, 'Config saved.');
}

async function saveQuickConfig() {
  let base = currentConfig || {};
  try {
    base = JSON.parse(editor.value || '{}');
  } catch (error) {
   validationBox.textContent = `高级 JSON 当前无效，无法合并常用配置：${error}`;
    validationBox.style.color = '#b91c1c';
    return;
  }
  const next = applyQuickForm(base);
  editor.value = JSON.stringify(next, null, 2);
  await postConfig(next, 'Common config saved. Adapter will hot-reload runtime_config.json.');
}

async function saveAndReloadConfig() {
  await saveQuickConfig();
}

async function generatePointIdMap() {
  const typePreset = document.getElementById('pointMapPreset')?.value || 'nav,point';
  const startIdValue = document.getElementById('pointMapStartId')?.value || '0';
  const response = await fetch('/api/point-id-map/generate', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      apply: true,
      types: typePreset.split(',').map((item) => item.trim()).filter(Boolean),
      startId: Number.parseInt(startIdValue, 10) || 0,
    }),
  });
  const data = await response.json();
  if (!response.ok || !data.ok) {
    validationBox.textContent = `自动生成 point_id_map 失败: ${data.error || response.statusText}`;
    validationBox.style.color = '#b91c1c';
    return;
  }
  currentConfig = data.config || currentConfig || {};
  setConfigForm(currentConfig, true);
  renderValidation(data.validation || { ok: true, errors: [], warnings: [] });
  validationBox.textContent = `已自动生成并保存 point_id_map，共 ${data.pointCount} 个点，类型：${(data.types || []).join(', ')}。\n${data.note || ''}\n\n` + validationBox.textContent;
  validationBox.style.color = '#166534';
  await loadStatus();
}

async function postConfig(config, successText) {
  const response = await fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config),
  });
  const data = await response.json();
  renderValidation(data.validation || { ok: false, errors: [data.error || '保存失败'], warnings: [] });
  if (response.ok) {
    currentConfig = config;
    setConfigForm(currentConfig, true);
    validationBox.textContent = `${successText}\n` + validationBox.textContent;
  }
  await loadStatus();
}

function setConfigForm(config, clean = false) {
  suppressConfigDirty = true;
  editor.value = JSON.stringify(config || {}, null, 2);
  populateQuickForm(config || {});
  suppressConfigDirty = false;
  if (clean) {
    quickFormDirty = false;
    editorDirty = false;
  }
}

function markQuickFormDirty() {
  if (!suppressConfigDirty) {
    quickFormDirty = true;
  }
}

function markEditorDirty() {
  if (!suppressConfigDirty) {
    editorDirty = true;
  }
}

function populateQuickForm(config) {
  quickFields.forEach(([id, key]) => {
    const element = document.getElementById(id);
    if (!element) return;
    const value = config[key];
    if (key === 'publish_rcs_event') {
      element.value = String(value !== false);
    } else if (value !== undefined && value !== null) {
      element.value = String(value);
    } else {
      element.value = '';
    }
  });
}

function applyQuickForm(config) {
  const next = { ...config };
  quickFields.forEach(([id, key]) => {
    const element = document.getElementById(id);
    if (!element) return;
    const raw = element.value.trim();
    if (key === 'distance_tolerance_m') {
      const numeric = Number(raw);
      if (!Number.isNaN(numeric)) next[key] = numeric;
    } else if (key === 'publish_rcs_event') {
      next[key] = raw === 'true';
    } else {
      next[key] = raw;
    }
  });
  return next;
}

function syncQuickFromJson() {
  try {
    const parsed = JSON.parse(editor.value || '{}');
    currentConfig = parsed;
    populateQuickForm(parsed);
    quickFormDirty = true;
    validationBox.textContent = '已从高级 JSON 刷新表单';
    validationBox.style.color = '#2563eb';
  } catch (error) {
    validationBox.textContent = `JSON 解析失败: ${error}`;
    validationBox.style.color = '#b91c1c';
  }
}

async function generateTopology() {
  const simplifyLevel = document.getElementById('topologySimplifySelect').value;
  const loopClosure = document.getElementById('topologyLoopClosureSelect').value === 'true';
  const collinearMode = document.getElementById('topologyCollinearSelect').value;
  const preset = { ...(simplifyPresets[simplifyLevel] || simplifyPresets.balanced) };
  if (!loopClosure) {
    preset.loopClosureEdges = 0;
  }
  if (collinearMode === 'off') {
    preset.collinearAngleDeg = 0;
  } else if (collinearMode === 'normal') {
    preset.collinearAngleDeg = 8;
  } else if (collinearMode === 'strong') {
    preset.collinearAngleDeg = 15;
  }
  const payload = {
    mapYaml: document.getElementById('mapYamlInput').value.trim(),
    mode: document.getElementById('topologyModeSelect').value,
    outputDir: document.getElementById('topologyOutputInput').value.trim(),
    modelName: document.getElementById('topologyModelNameInput').value.trim(),
    simplifyLevel,
    ...preset,
  };
  if (!payload.mapYaml) {
    topologyMessage.textContent = 'map.yaml 不能为空';
    topologyMessage.style.color = '#b91c1c';
    return;
  }
  topologyMessage.textContent = '正在生成...';
  topologyMessage.style.color = '#2563eb';
  const response = await fetch('/api/topology/generate', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  const data = await response.json();
  lastTopologyResult = data || {};
  topologyStatus.textContent = JSON.stringify(data || {}, null, 2);
  if (response.ok && data.ok) {
    topologyMessage.textContent = `Generation complete: ${data.points} points, ${data.paths} paths`;
    topologyMessage.style.color = '#166534';
    topologyPreview.src = `/api/topology/preview?file=${encodeURIComponent(data.previewFile || '')}&t=${Date.now()}`;
    topologyPreview.style.display = 'block';
    await loadBusinessPoints(false);
  } else {
    topologyMessage.textContent = data.error || data.stderr || '生成失败';
    topologyMessage.style.color = '#b91c1c';
  }
  await loadStatus();
}

async function loadTopologyToOpenTcs() {
  const plantJsonFile = lastTopologyResult.businessPlantJsonFile || lastBusinessPoints.businessPlantJsonFile || lastTopologyResult.plantJsonFile || lastBusinessPoints.plantJsonFile || "";
  if (!plantJsonFile) {
   topologyMessage.textContent = '请先生成拓扑，得到 opentcs_plant_model_candidate.json';
    topologyMessage.style.color = '#b91c1c';
    return;
  }
  const payload = {
    plantJsonFile,
    businessPlantJsonFile: lastTopologyResult.businessPlantJsonFile || '',
    openTcsBaseUrl: document.getElementById('quickOpenTcsUrl').value.trim() || currentConfig.open_tcs_base_url,
    vehicleName: document.getElementById('quickVehicleName').value.trim() || currentConfig.vehicle_name || 'Vehicle-01',
  };
  topologyMessage.textContent = '正在加载到 openTCS...';
  topologyMessage.style.color = '#2563eb';
  const response = await fetch('/api/topology/load-opentcs', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  const data = await response.json();
  topologyStatus.textContent = JSON.stringify(data || {}, null, 2);
  if (response.ok && data.ok) {
   topologyMessage.textContent = `已加载到 openTCS：${data.points} 点，${data.paths} 路径，${data.vehicles} 辆车`;
    topologyMessage.style.color = '#166534';
  } else {
    topologyMessage.textContent = [data.error || 'load to openTCS failed', data.url ? `URL: ${data.url}` : '', data.body ? `Response: ${data.body}` : '', data.hint || ''].filter(Boolean).join('\n');
    topologyMessage.style.color = '#b91c1c';
  }
  await loadStatus();
}

function formatOpenTcsErrorBody(body) {
  const text = String(body || '').trim();
  try {
    const parsed = JSON.parse(text);
    if (Array.isArray(parsed) && parsed.length) {
      return `模型中存在未知对象或旧对象引用：${parsed.join(', ')}`;
    }
    if (parsed && typeof parsed === 'object') {
      return parsed.message || parsed.error || text;
    }
  } catch (error) {
    // Keep the raw response when openTCS does not return JSON.
  }
  return text;
}
async function testConnections() {
  const payload = {
    mqttUri: document.getElementById('quickMqttUri').value.trim() || currentConfig.mqtt_uri,
    openTcsBaseUrl: document.getElementById('quickOpenTcsUrl').value.trim() || currentConfig.open_tcs_base_url,
    rcsBaseUrl: document.getElementById('quickRcsUrl').value.trim() || currentConfig.rcs_base_url,
  };
  connectionBox.textContent = '正在测试连接...';
  connectionBox.style.color = '#2563eb';
  const response = await fetch('/api/connection-test', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  const data = await response.json();
  const lines = [
    `MQTT: ${data.mqtt && data.mqtt.ok ? 'OK' : '失败'} ${formatConnectionDetail(data.mqtt)}`,
    `openTCS: ${data.openTcs && data.openTcs.ok ? 'OK' : '失败'} ${formatConnectionDetail(data.openTcs)}`,
    `RCS: ${data.rcs && data.rcs.ok ? 'OK' : '失败'} ${formatConnectionDetail(data.rcs)}`,
  ];
  connectionBox.textContent = lines.join('\n');
  connectionBox.style.color = lines.every((line) => line.includes('OK')) ? '#166534' : '#b91c1c';
}

function formatConnectionDetail(item) {
  if (!item) return '';
  if (item.status) return `status=${item.status}`;
  if (item.host) return `${item.host}:${item.port || ''}${item.error ? ` ${item.error}` : ''}`;
  return item.error || '';
}

async function loadBusinessPoints(showEmptyMessage = true) {
  const response = await fetch('/api/topology/points');
  const data = await response.json();
  lastBusinessPoints = data || {};
  renderBusinessPoints(data);
  if (showEmptyMessage) {
    businessMessage.textContent = data.ok
      ? `Loaded ${data.pointCount} points, ${data.pathCount} paths`
      : data.error || "Failed to load points";
    businessMessage.style.color = data.ok ? '#166534' : '#b91c1c';
  }
}

async function loadTopologyEditor() {
  const response = await fetch('/api/topology/points');
  const data = await response.json();
  if (!response.ok || !data.ok) {
    editMessage.textContent = data.error || '读取当前拓扑失败';
    editMessage.style.color = '#b91c1c';
    return;
  }
  editableTopology = {
    mappingFile: data.mappingFile || '',
    plantJsonFile: data.plantJsonFile || '',
    points: (data.points || []).map((point) => ({ ...point, originalName: point.name })),
    paths: (data.paths || []).map((path) => ({ ...path })),
  };
  renderTopologyEditor();
  editMessage.textContent = `Loaded ${editableTopology.points.length} points, ${editableTopology.paths.length} paths`;
  editMessage.style.color = '#166534';
}

function renderTopologyEditor() {
  renderEditPoints();
  renderEditPaths();
  refreshPathPointOptions();
  updateEditSummary();
}

function renderEditPoints() {
  editPointTable.innerHTML = '';
  editableTopology.points.forEach((point, index) => {
    const ros = point.ros_pose || {};
    const row = document.createElement('tr');
    row.innerHTML = `
      <td><input class="edit-point-name" data-index="${index}" value="${escapeHtml(point.name || '')}"></td>
      <td><input class="edit-point-kind" data-index="${index}" value="${escapeHtml(point.kind || 'manual')}"></td>
      <td><input class="edit-point-ros-x" data-index="${index}" type="number" step="0.001" value="${escapeHtml(String(ros.x ?? 0))}"></td>
      <td><input class="edit-point-ros-y" data-index="${index}" type="number" step="0.001" value="${escapeHtml(String(ros.y ?? 0))}"></td>
      <td><input class="edit-point-yaw" data-index="${index}" type="number" step="0.001" value="${escapeHtml(String(ros.yaw ?? 0))}"></td>
      <td><button class="delete-edit-point" data-index="${index}">删除</button></td>
    `;
    editPointTable.appendChild(row);
  });
}

function renderEditPaths() {
  editPathTable.innerHTML = '';
  editableTopology.paths.forEach((path, index) => {
    const row = document.createElement('tr');
    row.innerHTML = `
      <td><input class="edit-path-src" data-index="${index}" value="${escapeHtml(path.srcPointName || '')}"></td>
      <td><input class="edit-path-dest" data-index="${index}" value="${escapeHtml(path.destPointName || '')}"></td>
      <td><input class="edit-path-bidirectional" data-index="${index}" type="checkbox" ${path.bidirectional !== false ? 'checked' : ''}></td>
      <td><input class="edit-path-length" data-index="${index}" type="number" min="1" step="1" value="${escapeHtml(String(path.length_mm || ''))}"></td>
      <td><button class="delete-edit-path" data-index="${index}">删除</button></td>
    `;
    editPathTable.appendChild(row);
  });
}

function updateEditSummary() {
  editPointCount.textContent = `点位 ${editableTopology.points.length}`;
  editPathCount.textContent = `路径 ${editableTopology.paths.length}`;
}

function collectEditedTopology() {
  const points = Array.from(editPointTable.querySelectorAll('tr')).map((row, index) => {
    const oldPoint = editableTopology.points[index] || {};
    const pixel = oldPoint.pixel || {};
    return {
      originalName: oldPoint.originalName || oldPoint.name || '',
      name: row.querySelector('.edit-point-name').value.trim(),
      kind: row.querySelector('.edit-point-kind').value.trim() || 'manual',
      rosX: row.querySelector('.edit-point-ros-x').value,
      rosY: row.querySelector('.edit-point-ros-y').value,
      yaw: row.querySelector('.edit-point-yaw').value,
      pixelX: pixel.x || 0,
      pixelY: pixel.y || 0,
    };
  });
  const paths = Array.from(editPathTable.querySelectorAll('tr')).map((row) => ({
    srcPointName: row.querySelector('.edit-path-src').value.trim(),
    destPointName: row.querySelector('.edit-path-dest').value.trim(),
    bidirectional: row.querySelector('.edit-path-bidirectional').checked,
    length_mm: row.querySelector('.edit-path-length').value,
  }));
  editableTopology.points = points.map((point) => ({
    name: point.name,
    originalName: point.originalName,
    kind: point.kind,
    ros_pose: { x: Number(point.rosX || 0), y: Number(point.rosY || 0), yaw: Number(point.yaw || 0) },
    pixel: { x: Number(point.pixelX || 0), y: Number(point.pixelY || 0) },
  }));
  editableTopology.paths = paths;
  return {
    mappingFile: editableTopology.mappingFile || lastBusinessPoints.mappingFile || lastTopologyResult.mappingFile || '',
    plantJsonFile: editableTopology.plantJsonFile || lastBusinessPoints.plantJsonFile || lastTopologyResult.plantJsonFile || '',
    modelName: document.getElementById('editModelNameInput').value.trim() || 'agv-map-edited',
    revisionNote: document.getElementById('editRevisionInput').value.trim(),
    points,
    paths,
  };
}

function addEditPoint() {
  collectEditedTopology();
  const nextIndex = editableTopology.points.length + 1;
  editableTopology.points.push({
    name: `MANUAL_${String(nextIndex).padStart(2, '0')}`,
    originalName: '',
    kind: 'manual',
    ros_pose: { x: 0, y: 0, yaw: 0 },
    pixel: { x: 0, y: 0 },
  });
  renderTopologyEditor();
}

function addEditPath() {
  collectEditedTopology();
  const src = document.getElementById('newPathSrcSelect').value;
  const dest = document.getElementById('newPathDestSelect').value;
  if (!src || !dest || src === dest) {
    editMessage.textContent = '请选择不同的起点和终点';
    editMessage.style.color = '#b91c1c';
    return;
  }
  editableTopology.paths.push({
    srcPointName: src,
    destPointName: dest,
    bidirectional: document.getElementById('newPathBidirectional').checked,
    length_mm: '',
  });
  renderTopologyEditor();
}

function refreshPathPointOptions() {
  const payload = collectPointNamesFromEditor();
  ['newPathSrcSelect', 'newPathDestSelect'].forEach((id) => {
    const select = document.getElementById(id);
    const previous = select.value;
    select.innerHTML = '';
    payload.forEach((name) => {
      const option = document.createElement('option');
      option.value = name;
      option.textContent = name;
      select.appendChild(option);
    });
    if (payload.includes(previous)) {
      select.value = previous;
    }
  });
}

function collectPointNamesFromEditor() {
  const names = Array.from(editPointTable.querySelectorAll('.edit-point-name'))
    .map((input) => input.value.trim())
    .filter(Boolean);
  if (names.length) return names;
  return editableTopology.points.map((point) => point.name).filter(Boolean);
}

async function validateEditedTopology() {
  const payload = collectEditedTopology();
  updateEditSummary();
  const response = await fetch('/api/topology/validate-edits', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  const data = await response.json();
  renderEditValidation(data.validation || { ok: false, errors: [data.error || '校验失败'], warnings: [] });
  return response.ok && data.ok;
}

async function saveEditedTopology() {
  const payload = collectEditedTopology();
  updateEditSummary();
  editMessage.textContent = '正在保存编辑版拓扑...';
  editMessage.style.color = '#2563eb';
  const response = await fetch('/api/topology/save-edits', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  const data = await response.json();
  if (response.ok && data.ok) {
    lastTopologyResult = { ...lastTopologyResult, ...data };
    editableTopology.mappingFile = data.mappingFile || editableTopology.mappingFile;
    editableTopology.plantJsonFile = data.plantJsonFile || editableTopology.plantJsonFile;
    topologyStatus.textContent = JSON.stringify(data || {}, null, 2);
    renderEditValidation(data.validation);
    await loadBusinessPoints(false);
  } else {
    renderEditValidation(data.validation || { ok: false, errors: [data.error || '保存失败'], warnings: [] });
  }
}

function renderEditValidation(validation) {
  const ok = Boolean(validation && validation.ok);
  const errors = (validation && validation.errors) || [];
  const warnings = (validation && validation.warnings) || [];
  const lines = [
    ok ? '编辑拓扑校验通过' : '编辑拓扑校验失败',
    `点位: ${(validation && validation.pointCount) || 0}，路径: ${(validation && validation.pathCount) || 0}`,
    warnings.length ? ("Warnings:\\n- " + warnings.join("\\n- ")) : "Warnings: none",
    errors.length ? ("Errors:\\n- " + errors.join("\\n- ")) : "Errors: none",
  ];
  editMessage.textContent = lines.join('\n\n');
  editMessage.style.color = ok ? '#166534' : '#b91c1c';
}

function renderBusinessPoints(data) {
  businessPointTable.innerHTML = '';
  if (!data || !data.ok || !Array.isArray(data.points)) {
    return;
  }
  const savedRenames = lastTopologyResult.businessPointRenames || {};
  data.points.forEach((point) => {
    const row = document.createElement('tr');
    const originalName = point.name || '';
    const businessName = savedRenames[originalName] || originalName;
    row.innerHTML = `
      <td><code>${escapeHtml(originalName)}</code></td>
      <td><input class="business-name-input" data-original="${escapeHtml(originalName)}" value="${escapeHtml(businessName)}"></td>
      <td>${escapeHtml(point.kind || '')}</td>
      <td>${escapeHtml(String((point.ros_pose || {}).x ?? ''))}</td>
      <td>${escapeHtml(String((point.ros_pose || {}).y ?? ''))}</td>
      <td>${escapeHtml(`${(point.pixel || {}).x ?? ''},${(point.pixel || {}).y ?? ''}`)}</td>
    `;
    businessPointTable.appendChild(row);
  });
}

function escapeHtml(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#039;');
}

function fillBusinessPreset() {
  const presets = document.getElementById('businessPresetInput').value
    .split(',')
    .map((item) => item.trim())
    .filter(Boolean);
  const inputs = Array.from(document.querySelectorAll('.business-name-input'));
  inputs.forEach((input, index) => {
    if (presets[index]) {
      input.value = presets[index];
    }
  });
  businessMessage.textContent = `Filled ${Math.min(presets.length, inputs.length)} business point names.`;
  businessMessage.style.color = '#2563eb';
}

function collectBusinessRenames() {
  const renames = {};
  Array.from(document.querySelectorAll('.business-name-input')).forEach((input) => {
    const original = input.dataset.original;
    const value = input.value.trim();
    if (original) {
      renames[original] = value || original;
    }
  });
  return renames;
}

async function saveBusinessTopology() {
  if (!lastBusinessPoints.ok) {
    await loadBusinessPoints(false);
  }
  const payload = {
    mappingFile: lastBusinessPoints.mappingFile || lastTopologyResult.mappingFile,
    plantJsonFile: lastBusinessPoints.plantJsonFile || lastTopologyResult.plantJsonFile,
    renames: collectBusinessRenames(),
  };
  businessMessage.textContent = '正在保存业务点名...';
  businessMessage.style.color = '#2563eb';
  const response = await fetch('/api/topology/business', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  const data = await response.json();
  if (response.ok && data.ok) {
    lastTopologyResult = { ...lastTopologyResult, ...data };
    businessMessage.textContent = `已保存业务点名：重命名 ${data.renamedPoints} 个点`;
    businessMessage.style.color = '#166534';
    await loadTopology();
  } else {
    businessMessage.textContent = data.error || '保存业务点名失败';
    businessMessage.style.color = '#b91c1c';
  }
}

async function applyRuntimeMapping() {
  const payload = {
    businessMappingFile: lastTopologyResult.businessMappingFile || lastBusinessPoints.businessMappingFile || '',
    mappingFile: lastBusinessPoints.mappingFile || lastTopologyResult.mappingFile || '',
    mqttUri: document.getElementById('quickMqttUri').value.trim(),
    sourceTopic: document.getElementById('quickSourceTopic').value.trim(),
    commandTopic: document.getElementById('quickCommandTopic').value.trim(),
    openTcsBaseUrl: document.getElementById('quickOpenTcsUrl').value.trim(),
    rcsBaseUrl: document.getElementById('quickRcsUrl').value.trim(),
    vehicleName: document.getElementById('quickVehicleName').value.trim(),
    agvId: document.getElementById('quickAgvId').value.trim(),
    pointIdMap: document.getElementById('quickPointMap').value.trim(),
  };
  businessMessage.textContent = '正在应用到 adapter...';
  businessMessage.style.color = '#2563eb';
  const response = await fetch('/api/topology/apply-runtime', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  const data = await response.json();
  if (response.ok && data.ok) {
    currentConfig = data.config || currentConfig;
    setConfigForm(currentConfig, true);
    renderValidation(data.validation);
    businessMessage.textContent = `已应用到 adapter：${data.runtimeMappingFile}`;
    businessMessage.style.color = '#166534';
    await loadStatus();
  } else {
    businessMessage.textContent = data.error || '应用到 adapter 失败';
    if (data.validation) {
      businessMessage.textContent += `\n${JSON.stringify(data.validation, null, 2)}`;
    }
    businessMessage.style.color = '#b91c1c';
  }
}

async function uploadMapFiles() {
  const files = [
    ...Array.from(mapYamlUploadInput.files || []),
    ...Array.from(mapPgmUploadInput.files || []),
  ];
  const hasYaml = files.some((file) => /\.(ya?ml)$/i.test(file.name));
  const hasPgm = files.some((file) => /\.pgm$/i.test(file.name));
  if (!hasYaml || !hasPgm) {
    topologyMessage.textContent = '请选择 map.yaml 和 map.pgm';
    topologyMessage.style.color = '#b91c1c';
    return;
  }
  const body = new FormData();
  files.forEach((file) => body.append('files', file));
  topologyMessage.textContent = '正在上传...';
  topologyMessage.style.color = '#2563eb';
  const response = await fetch('/api/topology/upload', {
    method: 'POST',
    body,
  });
  const data = await response.json();
  topologyStatus.textContent = JSON.stringify(data || {}, null, 2);
  if (response.ok && data.ok) {
    document.getElementById('mapYamlInput').value = data.mapYaml || '';
    topologyMessage.textContent = `Upload complete: ${(data.savedFiles || []).length} files`;
    topologyMessage.style.color = '#166534';
  } else {
    topologyMessage.textContent = data.error || '上传失败';
    topologyMessage.style.color = '#b91c1c';
  }
  await loadStatus();
}

async function loadTemplate() {
  const name = document.getElementById('templateSelect').value;
  const response = await fetch(`/api/template?name=${encodeURIComponent(name)}`);
  const data = await response.json();
  currentConfig = data.config || {};
  setConfigForm(currentConfig, false);
  quickFormDirty = true;
  editorDirty = true;
  validationBox.textContent = `已载入模型：${name}`;
  validationBox.style.color = '#2563eb';
}

function renderValidation(validation) {
  const ok = Boolean(validation && validation.ok);
  const errors = (validation && validation.errors) || [];
  const warnings = (validation && validation.warnings) || [];
  const lines = [
    ok ? '校验通过' : '校验失败',
    warnings.length ? ("Warnings:\\n- " + warnings.join("\\n- ")) : "Warnings: none",
    errors.length ? ("Errors:\\n- " + errors.join("\\n- ")) : "Errors: none",
  ];
  validationBox.textContent = lines.join('\n\n');
  validationBox.style.color = ok ? '#166534' : '#b91c1c';
}

document.getElementById('reloadBtn').addEventListener('click', async () => {
  await saveAndReloadConfig();
});
document.getElementById('saveBtn').addEventListener('click', saveConfig);
document.getElementById('saveQuickBtn').addEventListener('click', saveQuickConfig);
document.getElementById('generatePointMapBtn').addEventListener('click', generatePointIdMap);
document.getElementById('syncQuickBtn').addEventListener('click', syncQuickFromJson);
document.getElementById('testConnectionBtn').addEventListener('click', testConnections);
document.getElementById('loadTemplateBtn').addEventListener('click', loadTemplate);
document.getElementById('uploadMapBtn').addEventListener('click', uploadMapFiles);
document.getElementById('generateTopologyBtn').addEventListener('click', generateTopology);
document.getElementById('loadOpenTcsBtn').addEventListener('click', loadTopologyToOpenTcs);
document.getElementById('reloadTopologyBtn').addEventListener('click', loadTopology);
document.getElementById('loadBusinessPointsBtn').addEventListener('click', () => loadBusinessPoints(true));
document.getElementById('fillBusinessPresetBtn').addEventListener('click', fillBusinessPreset);
document.getElementById('saveBusinessTopologyBtn').addEventListener('click', saveBusinessTopology);
document.getElementById('applyRuntimeBtn').addEventListener('click', applyRuntimeMapping);
document.getElementById('loadEditorBtn').addEventListener('click', loadTopologyEditor);
document.getElementById('addEditPointBtn').addEventListener('click', addEditPoint);
document.getElementById('addEditPathBtn').addEventListener('click', addEditPath);
document.getElementById('validateEditBtn').addEventListener('click', validateEditedTopology);
document.getElementById('saveEditBtn').addEventListener('click', saveEditedTopology);
editor.addEventListener('input', markEditorDirty);
quickFields.forEach(([id]) => {
  const element = document.getElementById(id);
  if (!element) return;
  element.addEventListener('input', markQuickFormDirty);
  element.addEventListener('change', markQuickFormDirty);
});
editPointTable.addEventListener('click', (event) => {
  if (!event.target.classList.contains('delete-edit-point')) return;
  collectEditedTopology();
  const index = Number(event.target.dataset.index);
  const removed = editableTopology.points[index];
  if (!removed) return;
  editableTopology.points.splice(index, 1);
  editableTopology.paths = editableTopology.paths.filter((path) => (
    path.srcPointName !== removed.name && path.destPointName !== removed.name
  ));
  renderTopologyEditor();
});
editPathTable.addEventListener('click', (event) => {
  if (!event.target.classList.contains('delete-edit-path')) return;
  collectEditedTopology();
  const index = Number(event.target.dataset.index);
  editableTopology.paths.splice(index, 1);
  renderTopologyEditor();
});
editPointTable.addEventListener('change', refreshPathPointOptions);

loadConfig();
loadStatus();
loadTopology();
setInterval(loadStatus, 3000);
