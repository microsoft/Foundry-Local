const state = { file: null, fileName: null, previewUrl: null };

function releasePreviewUrl() {
  if (state.previewUrl) {
    URL.revokeObjectURL(state.previewUrl);
    state.previewUrl = null;
  }
}

function bindUpload() {
  const fileInput = document.getElementById('fileInput');
  const chooseBtn = document.getElementById('chooseFileBtn');
  const dropzone = document.getElementById('dropzone');

  chooseBtn.addEventListener('click', () => fileInput.click());

  fileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (file) loadFile(file);
  });

  dropzone.addEventListener('dragover', (e) => { e.preventDefault(); dropzone.classList.add('drag-over'); });
  dropzone.addEventListener('dragleave', () => dropzone.classList.remove('drag-over'));
  dropzone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropzone.classList.remove('drag-over');
    const file = e.dataTransfer.files[0];
    if (file) loadFile(file);
  });
}

function loadFile(file) {
  state.file = file;
  state.fileName = file.name;

  const preview = document.getElementById('previewSection');
  const nameEl = document.getElementById('fileName');
  const player = document.getElementById('audioPlayer');

  nameEl.textContent = `${file.name} (${(file.size / 1024).toFixed(1)} KB)`;
  releasePreviewUrl();
  state.previewUrl = URL.createObjectURL(file);
  player.src = state.previewUrl;
  player.load();
  preview.style.display = 'block';
  document.getElementById('transcribeBtn').disabled = false;
}

async function transcribe() {
  const statusEl = document.getElementById('transcribeStatus');
  const btn = document.getElementById('transcribeBtn');

  if (!state.file) {
    statusEl.textContent = 'Please select an audio file first.';
    statusEl.classList.add('error');
    return;
  }

  statusEl.textContent = 'Transcribing\u2026';
  statusEl.classList.remove('error');
  btn.disabled = true;

  const format = document.getElementById('formatSelect').value;
  const form = new FormData();
  form.append('file', state.file, state.fileName);
  form.append('format', format);

  try {
    const res = await fetch('/v1/audio/transcriptions', { method: 'POST', body: form });
    if (!res.ok) {
      const txt = await res.text();
      throw new Error(txt || `HTTP ${res.status}`);
    }

    if (format === 'json') {
      const data = await res.json();
      renderResult(JSON.stringify(data, null, 2));
    } else {
      const text = await res.text();
      renderResult(text);
    }
    statusEl.textContent = 'Done — transcription complete.';
  } catch (err) {
    statusEl.textContent = `Error: ${err.message}`;
    statusEl.classList.add('error');
  } finally {
    btn.disabled = false;
  }
}

function renderResult(text) {
  const resultEl = document.getElementById('resultText');
  const copyBtn = document.getElementById('copyBtn');
  resultEl.textContent = text;
  copyBtn.style.display = 'inline-block';
}

function setupCopyButton() {
  const copyBtn = document.getElementById('copyBtn');
  const resultEl = document.getElementById('resultText');
  copyBtn.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(resultEl.textContent);
      const orig = copyBtn.textContent;
      copyBtn.textContent = 'Copied!';
      copyBtn.classList.add('success');
      setTimeout(() => { copyBtn.textContent = orig; copyBtn.classList.remove('success'); }, 2000);
    } catch { alert('Failed to copy'); }
  });
}

async function checkHealth() {
  try {
    const res = await fetch('/api/health/status');
    if (res.ok) {
      const data = await res.json();
      document.getElementById('stat-status').textContent = data.status || 'Unknown';
      document.getElementById('stat-model').textContent = data.model || '—';
      document.getElementById('stat-cached').textContent = data.cached ? 'Yes' : 'No';
    } else {
      document.getElementById('stat-status').textContent = 'Degraded';
    }
  } catch {
    document.getElementById('stat-status').textContent = 'Offline';
  }
}

document.addEventListener('DOMContentLoaded', () => {
  bindUpload();
  setupCopyButton();
  document.getElementById('transcribeBtn').addEventListener('click', transcribe);
  checkHealth();
});

window.addEventListener('beforeunload', releasePreviewUrl);
