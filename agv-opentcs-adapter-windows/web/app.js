const editor = document.getElementById('configEditor');
const validationBox = document.getElementById('validationBox');
const adapterStatus = document.getElementById('adapterStatus');
const simStatus = document.getElementById('simStatus');
const webStatus = document.getElementById('webStatus');

async function loadConfig() {
  const response = await fetch('/api/config');
  const data = await response.json();
  editor.value = JSON.stringify(data.config, null, 2);
  renderValidation(data.validation);
}

async function loadStatus() {
  const response = await fetch('/api/status');
  const data = await response.json();
  adapterStatus.textContent = JSON.stringify(data.adapter || {}, null, 2);
  simStatus.textContent = JSON.stringify(data.simulator || {}, null, 2);
  webStatus.textContent = JSON.stringify(data.web || {}, null, 2);
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
  const response = await fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(parsed),
  });
  const data = await response.json();
  renderValidation(data.validation || { ok: false, errors: [data.error || '保存失败'], warnings: [] });
  if (response.ok) {
    validationBox.textContent = '保存成功。\n' + validationBox.textContent;
  }
  await loadStatus();
}

async function loadTemplate() {
  const name = document.getElementById('templateSelect').value;
  const response = await fetch(`/api/template?name=${encodeURIComponent(name)}`);
  const data = await response.json();
  editor.value = JSON.stringify(data.config, null, 2);
  validationBox.textContent = `已载入模板: ${name}`;
  validationBox.style.color = '#2563eb';
}

function renderValidation(validation) {
  const ok = Boolean(validation && validation.ok);
  const errors = (validation && validation.errors) || [];
  const warnings = (validation && validation.warnings) || [];
  const lines = [
    ok ? '校验通过' : '校验失败',
    warnings.length ? `警告:\n- ${warnings.join('\n- ')}` : '警告: 无',
    errors.length ? `错误:\n- ${errors.join('\n- ')}` : '错误: 无',
  ];
  validationBox.textContent = lines.join('\n\n');
  validationBox.style.color = ok ? '#166534' : '#b91c1c';
}

document.getElementById('reloadBtn').addEventListener('click', async () => {
  await loadConfig();
  await loadStatus();
});
document.getElementById('saveBtn').addEventListener('click', saveConfig);
document.getElementById('loadTemplateBtn').addEventListener('click', loadTemplate);

loadConfig();
loadStatus();
setInterval(loadStatus, 3000);
