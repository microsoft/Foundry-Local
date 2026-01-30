// =====================================================
// Foundry Local Chat - Renderer Process
// =====================================================

// Simple markdown parser with code block handling
const SimpleMarkdown = {
  parse(text) {
    if (!text) return '';
    
    let html = this.escapeHtml(text);
    
    // Code blocks with language
    html = html.replace(/```(\w*)\n([\s\S]*?)```/g, (match, lang, code) => {
      return `<div class="code-block-wrapper">
        <button class="code-copy-btn" data-copy="true" title="Copy code">
          <span class="copy-icon">⧉</span>
          <span class="check-icon">✓</span>
        </button>
        <pre><code class="language-${lang || 'plaintext'}">${code.trim()}</code></pre>
      </div>`;
    });
    
    // Inline code (must come before other formatting)
    html = html.replace(/`([^`]+)`/g, '<code>$1</code>');
    
    // Headings (### before ## before #)
    html = html.replace(/^### (.+)$/gm, '<h4>$1</h4>');
    html = html.replace(/^## (.+)$/gm, '<h3>$1</h3>');
    html = html.replace(/^# (.+)$/gm, '<h2>$1</h2>');
    
    // Also handle headings after <br> tags (from previous newline conversion)
    html = html.replace(/<br>### (.+?)(<br>|$)/g, '<br><h4>$1</h4>$2');
    html = html.replace(/<br>## (.+?)(<br>|$)/g, '<br><h3>$1</h3>$2');
    html = html.replace(/<br># (.+?)(<br>|$)/g, '<br><h2>$1</h2>$2');
    
    // Unordered lists
    html = html.replace(/^- (.+)$/gm, '<li>$1</li>');
    html = html.replace(/(<li>.*<\/li>\n?)+/g, '<ul>$&</ul>');
    
    // Bold
    html = html.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
    
    // Italic
    html = html.replace(/\*([^*]+)\*/g, '<em>$1</em>');
    
    // Links
    html = html.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2" target="_blank">$1</a>');
    
    // Line breaks (but not inside block elements)
    html = html.replace(/\n/g, '<br>');
    
    // Clean up extra <br> around block elements
    html = html.replace(/<br>(<h[234]>)/g, '$1');
    html = html.replace(/(<\/h[234]>)<br>/g, '$1');
    html = html.replace(/<br>(<ul>)/g, '$1');
    html = html.replace(/(<\/ul>)<br>/g, '$1');
    
    return html;
  },
  
  escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }
};

// Copy code to clipboard - use event delegation
document.addEventListener('click', async (e) => {
  const button = e.target.closest('.code-copy-btn');
  if (!button) return;
  
  const codeBlock = button.closest('.code-block-wrapper').querySelector('code');
  const text = codeBlock.textContent;
  
  try {
    await navigator.clipboard.writeText(text);
    button.classList.add('copied');
    setTimeout(() => button.classList.remove('copied'), 2000);
  } catch (err) {
    console.error('Failed to copy:', err);
  }
});

// Estimate tokens from text (rough approximation: ~4 chars per token)
function estimateTokens(text) {
  return Math.ceil(text.length / 4);
}

// Calculate total context tokens from all messages
function calculateContextTokens() {
  return messages.reduce((total, msg) => total + estimateTokens(msg.content), 0);
}

// Update context usage display
function updateContextUsage() {
  contextTokens = calculateContextTokens();
  const percentage = Math.min(100, Math.round((contextTokens / CONTEXT_LIMIT) * 100));
  
  contextFill.style.width = `${percentage}%`;
  contextLabel.textContent = `${percentage}%`;
  
  // Update color based on usage
  contextFill.classList.remove('warning', 'danger');
  if (percentage >= 90) {
    contextFill.classList.add('danger');
  } else if (percentage >= 70) {
    contextFill.classList.add('warning');
  }
  
  // Update tooltip
  contextUsage.title = `Context: ${contextTokens.toLocaleString()} / ${CONTEXT_LIMIT.toLocaleString()} tokens (~${percentage}%)`;
}

// State
let messages = [];
let currentModelAlias = null;
let isGenerating = false;
let contextTokens = 0;
const CONTEXT_LIMIT = 8192; // Default context window, will update based on model

// DOM Elements
const sidebar = document.getElementById('sidebar');
const sidebarToggle = document.getElementById('sidebarToggle');
const mobileMenuBtn = document.getElementById('mobileMenuBtn');
const modelList = document.getElementById('modelList');
const refreshModels = document.getElementById('refreshModels');
const modelBadge = document.getElementById('modelBadge');
const chatMessages = document.getElementById('chatMessages');
const chatForm = document.getElementById('chatForm');
const messageInput = document.getElementById('messageInput');
const sendBtn = document.getElementById('sendBtn');
const newChatBtn = document.getElementById('newChatBtn');
const toastContainer = document.getElementById('toastContainer');
const recordBtn = document.getElementById('recordBtn');
const transcriptionSettingsBtn = document.getElementById('transcriptionSettingsBtn');
const whisperModal = document.getElementById('whisperModal');
const whisperModelList = document.getElementById('whisperModelList');
const whisperModalCancel = document.getElementById('whisperModalCancel');
const currentWhisperModelEl = document.getElementById('currentWhisperModel');
const contextFill = document.getElementById('contextFill');
const contextLabel = document.getElementById('contextLabel');
const contextUsage = document.getElementById('contextUsage');

// Recording state
let mediaRecorder = null;
let audioChunks = [];
let isRecording = false;
let selectedWhisperModel = null;

// Initialize
document.addEventListener('DOMContentLoaded', async () => {
  setupEventListeners();
  setupSidebarResize();
  setupRecordButton();
  updateContextUsage();
  await loadModels();
  setupChatChunkListener();
});

function setupSidebarResize() {
  const resizeHandle = document.getElementById('sidebarResizeHandle');
  let isResizing = false;
  
  resizeHandle.addEventListener('mousedown', (e) => {
    isResizing = true;
    resizeHandle.classList.add('dragging');
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none';
  });
  
  document.addEventListener('mousemove', (e) => {
    if (!isResizing) return;
    const newWidth = Math.min(Math.max(e.clientX, 240), 480);
    sidebar.style.width = newWidth + 'px';
  });
  
  document.addEventListener('mouseup', () => {
    if (isResizing) {
      isResizing = false;
      resizeHandle.classList.remove('dragging');
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    }
  });
}

function setupRecordButton() {
  recordBtn.addEventListener('click', handleRecordClick);
  transcriptionSettingsBtn.addEventListener('click', openTranscriptionSettings);
  whisperModalCancel.addEventListener('click', () => {
    whisperModal.classList.remove('visible');
  });
}

async function openTranscriptionSettings() {
  const whisperModels = await window.foundryAPI.getWhisperModels();
  showWhisperModal(whisperModels, true);
}

async function handleRecordClick() {
  if (isRecording) {
    // Stop recording
    stopRecording();
  } else {
    // Check if whisper model is available
    const whisperModels = await window.foundryAPI.getWhisperModels();
    const cachedModels = whisperModels.filter(m => m.isCached);
    
    if (cachedModels.length === 0) {
      // Show modal to download whisper model
      showWhisperModal(whisperModels, false);
    } else {
      // Start recording
      startRecording();
    }
  }
}

function showWhisperModal(models, isSettings = false) {
  // Update current model display
  const cachedModels = models.filter(m => m.isCached);
  if (cachedModels.length > 0) {
    const current = selectedWhisperModel || cachedModels.sort((a, b) => (a.fileSizeMb || 0) - (b.fileSizeMb || 0))[0].alias;
    currentWhisperModelEl.innerHTML = `
      <span class="label">Current model:</span>
      <span class="model-name">${current}</span>
    `;
  } else {
    currentWhisperModelEl.innerHTML = `
      <span class="label">Current model:</span>
      <span class="model-name">None - download a model below</span>
    `;
  }
  
  whisperModelList.innerHTML = '';
  
  models.forEach(model => {
    const sizeStr = model.fileSizeMb ? `${(model.fileSizeMb / 1024).toFixed(1)} GB` : '';
    const isSelected = selectedWhisperModel === model.alias;
    const item = document.createElement('div');
    item.className = 'whisper-model-item' + (isSelected ? ' selected' : '');
    item.innerHTML = `
      <div class="model-info">
        <span class="model-name">${model.alias}</span>
        <span class="model-size">${sizeStr}</span>
      </div>
      <div class="model-actions">
        ${model.isCached 
          ? `<button class="use-btn">${isSelected ? '✓ Selected' : 'Select'}</button>
             <button class="delete-btn" title="Delete from cache">
               <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                 <polyline points="3 6 5 6 21 6"></polyline>
                 <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
               </svg>
             </button>`
          : '<button class="download-btn">Download</button>'
        }
      </div>
    `;
    
    if (model.isCached) {
      const useBtn = item.querySelector('.use-btn');
      useBtn.addEventListener('click', () => {
        selectedWhisperModel = model.alias;
        showToast(`Selected ${model.alias} for transcription`, 'success');
        // Refresh modal to show selection
        showWhisperModal(models, true);
      });
      
      const deleteBtn = item.querySelector('.delete-btn');
      deleteBtn.addEventListener('click', async () => {
        if (confirm(`Delete ${model.alias} from cache?`)) {
          try {
            await window.foundryAPI.deleteModel(model.alias);
            if (selectedWhisperModel === model.alias) {
              selectedWhisperModel = null;
            }
            showToast(`Deleted ${model.alias}`, 'success');
            const updatedModels = await window.foundryAPI.getWhisperModels();
            showWhisperModal(updatedModels, true);
          } catch (error) {
            showToast('Delete failed: ' + error.message, 'error');
          }
        }
      });
    } else {
      const downloadBtn = item.querySelector('.download-btn');
      downloadBtn.addEventListener('click', async () => {
        downloadBtn.textContent = 'Downloading...';
        downloadBtn.disabled = true;
        try {
          await window.foundryAPI.downloadWhisperModel(model.alias);
          showToast(`Downloaded ${model.alias}`, 'success');
          selectedWhisperModel = model.alias;
          const updatedModels = await window.foundryAPI.getWhisperModels();
          showWhisperModal(updatedModels, true);
        } catch (error) {
          showToast('Download failed: ' + error.message, 'error');
          downloadBtn.textContent = 'Download';
          downloadBtn.disabled = false;
        }
      });
    }
    
    whisperModelList.appendChild(item);
  });
  
  whisperModal.classList.add('visible');
}

async function startRecording() {
  try {
    // Request 16kHz mono audio for Whisper compatibility
    const stream = await navigator.mediaDevices.getUserMedia({ 
      audio: {
        sampleRate: 16000,
        channelCount: 1,
        echoCancellation: true,
        noiseSuppression: true
      } 
    });
    
    mediaRecorder = new MediaRecorder(stream);
    audioChunks = [];
    
    mediaRecorder.ondataavailable = (e) => {
      audioChunks.push(e.data);
    };
    
    mediaRecorder.onstop = async () => {
      // Stop all tracks
      stream.getTracks().forEach(track => track.stop());
      
      // Create audio blob
      const audioBlob = new Blob(audioChunks, { type: mediaRecorder.mimeType });
      await transcribeAudio(audioBlob);
    };
    
    mediaRecorder.start();
    isRecording = true;
    recordBtn.classList.add('recording');
    showToast('Recording... Click stop when done', 'warning');
  } catch (error) {
    console.error('Failed to start recording:', error);
    showToast('Failed to access microphone', 'error');
  }
}

function stopRecording() {
  if (mediaRecorder && isRecording) {
    mediaRecorder.stop();
    isRecording = false;
    recordBtn.classList.remove('recording');
    recordBtn.classList.add('transcribing');
  }
}

// Convert audio blob to 16kHz mono WAV format for Whisper
async function convertToWav(audioBlob) {
  const audioContext = new AudioContext();
  const arrayBuffer = await audioBlob.arrayBuffer();
  const audioBuffer = await audioContext.decodeAudioData(arrayBuffer);
  
  // Resample to 16kHz mono
  const targetSampleRate = 16000;
  const offlineContext = new OfflineAudioContext(1, audioBuffer.duration * targetSampleRate, targetSampleRate);
  
  const source = offlineContext.createBufferSource();
  source.buffer = audioBuffer;
  source.connect(offlineContext.destination);
  source.start(0);
  
  const resampledBuffer = await offlineContext.startRendering();
  
  // Convert to WAV
  const wavBuffer = audioBufferToWav(resampledBuffer);
  return new Blob([wavBuffer], { type: 'audio/wav' });
}

// Encode AudioBuffer to 16-bit PCM WAV format
function audioBufferToWav(buffer) {
  const numChannels = 1; // Force mono
  const sampleRate = buffer.sampleRate;
  const bitDepth = 16;
  
  const bytesPerSample = bitDepth / 8;
  const blockAlign = numChannels * bytesPerSample;
  
  // Get mono channel (mix down if stereo)
  let monoData;
  if (buffer.numberOfChannels === 1) {
    monoData = buffer.getChannelData(0);
  } else {
    // Mix stereo to mono
    const left = buffer.getChannelData(0);
    const right = buffer.getChannelData(1);
    monoData = new Float32Array(left.length);
    for (let i = 0; i < left.length; i++) {
      monoData[i] = (left[i] + right[i]) / 2;
    }
  }
  
  const samples = monoData.length;
  const dataSize = samples * blockAlign;
  const bufferSize = 44 + dataSize;
  
  const arrayBuffer = new ArrayBuffer(bufferSize);
  const view = new DataView(arrayBuffer);
  
  // RIFF header
  writeString(view, 0, 'RIFF');
  view.setUint32(4, 36 + dataSize, true);
  writeString(view, 8, 'WAVE');
  
  // fmt chunk
  writeString(view, 12, 'fmt ');
  view.setUint32(16, 16, true); // chunk size
  view.setUint16(20, 1, true);  // PCM format
  view.setUint16(22, numChannels, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * blockAlign, true);
  view.setUint16(32, blockAlign, true);
  view.setUint16(34, bitDepth, true);
  
  // data chunk
  writeString(view, 36, 'data');
  view.setUint32(40, dataSize, true);
  
  // Write audio data as 16-bit PCM
  let offset = 44;
  for (let i = 0; i < samples; i++) {
    const sample = Math.max(-1, Math.min(1, monoData[i]));
    const intSample = sample < 0 ? sample * 0x8000 : sample * 0x7FFF;
    view.setInt16(offset, intSample, true);
    offset += 2;
  }
  
  return arrayBuffer;
}

function writeString(view, offset, string) {
  for (let i = 0; i < string.length; i++) {
    view.setUint8(offset + i, string.charCodeAt(i));
  }
}

async function transcribeAudio(audioBlob) {
  try {
    showToast('Converting audio...', 'warning');
    
    // Convert to 16kHz mono WAV format for Whisper compatibility
    let wavBlob;
    try {
      wavBlob = await convertToWav(audioBlob);
    } catch (e) {
      console.error('WAV conversion failed:', e);
      showToast('Audio conversion failed: ' + e.message, 'error');
      recordBtn.classList.remove('transcribing');
      return;
    }
    
    showToast('Transcribing audio...', 'warning');
    
    // Convert blob to base64
    const arrayBuffer = await wavBlob.arrayBuffer();
    const uint8Array = new Uint8Array(arrayBuffer);
    
    // Use chunked base64 encoding for large arrays
    let base64 = '';
    const chunkSize = 32768;
    for (let i = 0; i < uint8Array.length; i += chunkSize) {
      const chunk = uint8Array.subarray(i, i + chunkSize);
      base64 += String.fromCharCode.apply(null, chunk);
    }
    base64 = btoa(base64);
    
    const tempPath = `/tmp/foundry_audio_${Date.now()}.wav`;
    
    const result = await window.foundryAPI.transcribeAudio(tempPath, base64);
    
    // Insert transcribed text into input
    const text = result.text || result.Text || '';
    if (text) {
      messageInput.value += text;
      messageInput.dispatchEvent(new Event('input'));
      showToast('Transcription complete', 'success');
    } else {
      showToast('No speech detected', 'warning');
    }
  } catch (error) {
    console.error('Transcription failed:', error);
    showToast('Transcription failed: ' + error.message, 'error');
  } finally {
    recordBtn.classList.remove('transcribing');
  }
}

function setupEventListeners() {
  // Sidebar toggle
  sidebarToggle.addEventListener('click', () => {
    sidebar.classList.toggle('collapsed');
  });
  
  mobileMenuBtn.addEventListener('click', () => {
    sidebar.classList.toggle('open');
  });
  
  // Refresh models
  refreshModels.addEventListener('click', async () => {
    refreshModels.classList.add('spinning');
    await loadModels();
    refreshModels.classList.remove('spinning');
  });
  
  // Chat form
  chatForm.addEventListener('submit', handleSendMessage);
  
  // Textarea auto-resize
  messageInput.addEventListener('input', () => {
    messageInput.style.height = 'auto';
    messageInput.style.height = Math.min(messageInput.scrollHeight, 150) + 'px';
  });
  
  // Enter to send, Shift+Enter for new line
  messageInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      chatForm.dispatchEvent(new Event('submit'));
    }
  });
  
  // New chat
  newChatBtn.addEventListener('click', clearChat);
  
  // Close sidebar on outside click (mobile)
  document.addEventListener('click', (e) => {
    if (window.innerWidth <= 768 && 
        sidebar.classList.contains('open') &&
        !sidebar.contains(e.target) &&
        !mobileMenuBtn.contains(e.target)) {
      sidebar.classList.remove('open');
    }
  });
}

function setupChatChunkListener() {
  window.foundryAPI.onChatChunk((data) => {
    if (data.content) {
      appendToLastAssistantMessage(data.content);
    }
  });
}

// Model Management
async function loadModels() {
  modelList.innerHTML = `
    <div class="loading-spinner">
      <div class="spinner"></div>
      <span>Loading models...</span>
    </div>
  `;
  
  try {
    const models = await window.foundryAPI.getModels();
    
    if (!models || models.length === 0) {
      modelList.innerHTML = `
        <div class="loading-spinner">
          <span>No models found</span>
        </div>
      `;
      return;
    }
    
    // Filter out whisper/audio models - only show chat models
    const chatModels = models.filter(m => {
      const alias = m.alias.toLowerCase();
      // Exclude whisper and other audio models
      if (alias.includes('whisper')) return false;
      return true;
    });
    
    const displayModels = chatModels;
    
    // Sort: cached first, then by name
    displayModels.sort((a, b) => {
      if (a.isCached && !b.isCached) return -1;
      if (!a.isCached && b.isCached) return 1;
      return a.alias.localeCompare(b.alias);
    });
    
    // Group by cached status
    const cachedModels = displayModels.filter(m => m.isCached);
    const availableModels = displayModels.filter(m => !m.isCached);
    
    modelList.innerHTML = '';
    
    if (cachedModels.length > 0) {
      const cachedGroup = document.createElement('div');
      cachedGroup.className = 'model-group';
      cachedGroup.innerHTML = `
        <div class="model-group-header">
          <div class="status-dot cached"></div>
          <span>Downloaded</span>
        </div>
      `;
      cachedModels.forEach(model => {
        cachedGroup.appendChild(createModelItem(model));
      });
      modelList.appendChild(cachedGroup);
    }
    
    if (availableModels.length > 0) {
      const availableGroup = document.createElement('div');
      availableGroup.className = 'model-group';
      availableGroup.innerHTML = `
        <div class="model-group-header">
          <div class="status-dot"></div>
          <span>Available</span>
        </div>
      `;
      availableModels.forEach(model => {
        availableGroup.appendChild(createModelItem(model));
      });
      modelList.appendChild(availableGroup);
    }
    
    if (displayModels.length === 0) {
      modelList.innerHTML = `
        <div class="loading-spinner">
          <span>No models available</span>
        </div>
      `;
    }
  } catch (error) {
    console.error('Failed to load models:', error);
    modelList.innerHTML = `
      <div class="loading-spinner">
        <span>Failed to load models</span>
        <span style="font-size: 11px; color: var(--error);">${error.message || error}</span>
      </div>
    `;
    showToast('Failed to load models: ' + error.message, 'error');
  }
}

function createModelItem(model) {
  const variant = model.variants[0];
  const item = document.createElement('div');
  item.className = 'model-item';
  const isActive = model.alias === currentModelAlias;
  if (isActive) {
    item.classList.add('active');
  }
  
  const sizeMb = variant?.fileSizeMb;
  const sizeStr = sizeMb ? `${(sizeMb / 1024).toFixed(1)} GB` : '';
  
  let statusHtml;
  if (isActive) {
    statusHtml = `
      <button class="unload-btn">Unload</button>
    `;
  } else if (model.isCached) {
    statusHtml = `
      <button class="delete-model-btn" title="Delete from cache">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <polyline points="3 6 5 6 21 6"></polyline>
          <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
          <line x1="10" y1="11" x2="10" y2="17"></line>
          <line x1="14" y1="11" x2="14" y2="17"></line>
        </svg>
      </button>
      <button class="load-btn">Load</button>
    `;
  } else {
    statusHtml = '<button class="download-btn">Download</button>';
  }
  
  item.innerHTML = `
    <div class="model-icon">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <path d="M21 16V8a2 2 0 00-1-1.73l-7-4a2 2 0 00-2 0l-7 4A2 2 0 003 8v8a2 2 0 001 1.73l7 4a2 2 0 002 0l7-4A2 2 0 0021 16z"/>
        <polyline points="3.27 6.96 12 12.01 20.73 6.96"/>
        <line x1="12" y1="22.08" x2="12" y2="12"/>
      </svg>
    </div>
    <div class="model-info">
      <div class="model-name">${model.alias}</div>
      <div class="model-size">${sizeStr}</div>
    </div>
    <div class="model-status">
      ${statusHtml}
    </div>
  `;
  
  // Handle click events
  if (isActive) {
    const unloadBtn = item.querySelector('.unload-btn');
    unloadBtn.addEventListener('click', async (e) => {
      e.stopPropagation();
      await unloadModel();
    });
  } else if (model.isCached) {
    const loadBtn = item.querySelector('.load-btn');
    loadBtn.addEventListener('click', async (e) => {
      e.stopPropagation();
      await loadModel(model.alias);
    });
    
    const deleteBtn = item.querySelector('.delete-model-btn');
    deleteBtn.addEventListener('click', async (e) => {
      e.stopPropagation();
      if (confirm(`Delete ${model.alias} from cache?`)) {
        try {
          await window.foundryAPI.deleteModel(model.alias);
          showToast(`Deleted ${model.alias}`, 'success');
          await loadModels();
        } catch (error) {
          showToast('Delete failed: ' + error.message, 'error');
        }
      }
    });
  } else {
    const downloadBtn = item.querySelector('.download-btn');
    downloadBtn.addEventListener('click', async (e) => {
      e.stopPropagation();
      await downloadModel(model.alias, item);
    });
  }
  
  return item;
}

async function downloadModel(alias, itemElement) {
  const statusEl = itemElement.querySelector('.model-status');
  statusEl.innerHTML = '<div class="status-indicator loading"></div>';
  
  try {
    showToast(`Downloading ${alias}...`, 'warning');
    await window.foundryAPI.downloadModel(alias);
    showToast(`Downloaded ${alias}. Loading...`, 'success');
    await loadModels();
    // Auto-load the model after download
    await loadModel(alias);
  } catch (error) {
    console.error('Download failed:', error);
    showToast('Download failed: ' + error.message, 'error');
    await loadModels();
  }
}

async function loadModel(alias) {
  if (isGenerating) {
    showToast('Please wait for the current response to finish', 'warning');
    return;
  }
  
  // Update UI to show loading
  const items = modelList.querySelectorAll('.model-item');
  items.forEach(item => {
    item.classList.remove('active');
    const nameEl = item.querySelector('.model-name');
    if (nameEl.textContent.includes(alias) || item.dataset.alias === alias) {
      item.classList.add('loading');
    }
  });
  
  try {
    showToast(`Loading ${alias}...`, 'warning');
    await window.foundryAPI.loadModel(alias);
    currentModelAlias = alias;
    
    // Update UI
    updateCurrentModelDisplay(alias);
    enableChat();
    showToast(`Model ${alias} loaded`, 'success');
    
    // Refresh model list to update active state
    await loadModels();
  } catch (error) {
    console.error('Failed to load model:', error);
    showToast('Failed to load model: ' + error.message, 'error');
    await loadModels();
  }
}

async function unloadModel() {
  if (isGenerating) {
    showToast('Please wait for the current response to finish', 'warning');
    return;
  }
  
  try {
    showToast('Unloading model...', 'warning');
    await window.foundryAPI.unloadModel();
    currentModelAlias = null;
    
    // Update UI
    modelBadge.textContent = 'Select a model to start';
    disableChat();
    showToast('Model unloaded', 'success');
    
    // Refresh model list
    await loadModels();
  } catch (error) {
    console.error('Failed to unload model:', error);
    showToast('Failed to unload model: ' + error.message, 'error');
  }
}

function updateCurrentModelDisplay(alias) {
  modelBadge.textContent = alias;
}

function enableChat() {
  messageInput.disabled = false;
  sendBtn.disabled = false;
  messageInput.placeholder = 'Type your message...';
  messageInput.focus();
}

function disableChat() {
  messageInput.disabled = true;
  sendBtn.disabled = true;
  messageInput.placeholder = 'Select a model to start chatting...';
}

// Chat Management
async function handleSendMessage(e) {
  e.preventDefault();
  
  const content = messageInput.value.trim();
  if (!content || isGenerating || !currentModelAlias) return;
  
  // Clear welcome message if present
  const welcomeMessage = chatMessages.querySelector('.welcome-message');
  if (welcomeMessage) {
    welcomeMessage.remove();
  }
  
  // Add user message
  messages.push({ role: 'user', content });
  addMessageToChat('user', content);
  updateContextUsage();
  
  // Clear input
  messageInput.value = '';
  messageInput.style.height = 'auto';
  
  // Disable send button
  isGenerating = true;
  sendBtn.disabled = true;
  
  // Add typing indicator
  const typingEl = addTypingIndicator();
  
  try {
    // Make API call
    const result = await window.foundryAPI.chat(messages);
    
    // Remove typing indicator
    typingEl.remove();
    
    // Add assistant message (content was already streamed, just add stats)
    messages.push({ role: 'assistant', content: result.content });
    updateLastAssistantMessageStats(result.stats);
    updateContextUsage();
    
  } catch (error) {
    console.error('Chat error:', error);
    typingEl.remove();
    showToast('Chat error: ' + error.message, 'error');
  } finally {
    isGenerating = false;
    sendBtn.disabled = false;
    messageInput.focus();
  }
}

function addMessageToChat(role, content) {
  const messageEl = document.createElement('div');
  messageEl.className = `message ${role}`;
  
  const avatar = role === 'user' ? 'U' : 
    `<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <path d="M21 16V8a2 2 0 00-1-1.73l-7-4a2 2 0 00-2 0l-7 4A2 2 0 003 8v8a2 2 0 001 1.73l7 4a2 2 0 002 0l7-4A2 2 0 0021 16z"/>
    </svg>`;
  
  messageEl.innerHTML = `
    <div class="message-avatar">${avatar}</div>
    <div class="message-content">
      <div class="message-bubble">${role === 'user' ? SimpleMarkdown.escapeHtml(content) : SimpleMarkdown.parse(content)}</div>
      ${role === 'assistant' ? '<div class="message-stats"></div>' : ''}
    </div>
  `;
  
  chatMessages.appendChild(messageEl);
  scrollToBottom();
  
  return messageEl;
}

function addTypingIndicator() {
  const typingEl = document.createElement('div');
  typingEl.className = 'message assistant';
  typingEl.id = 'typing-indicator';
  typingEl.innerHTML = `
    <div class="message-avatar">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <path d="M21 16V8a2 2 0 00-1-1.73l-7-4a2 2 0 00-2 0l-7 4A2 2 0 003 8v8a2 2 0 001 1.73l7 4a2 2 0 002 0l7-4A2 2 0 0021 16z"/>
      </svg>
    </div>
    <div class="message-content">
      <div class="typing-indicator">
        <span></span>
        <span></span>
        <span></span>
      </div>
    </div>
  `;
  chatMessages.appendChild(typingEl);
  scrollToBottom();
  return typingEl;
}

let currentAssistantMessage = null;
let currentAssistantContent = '';

function appendToLastAssistantMessage(content) {
  // If there's a typing indicator, replace it with actual message
  const typingIndicator = document.getElementById('typing-indicator');
  if (typingIndicator) {
    typingIndicator.remove();
    currentAssistantMessage = addMessageToChat('assistant', '');
    currentAssistantContent = '';
  }
  
  if (!currentAssistantMessage) {
    currentAssistantMessage = addMessageToChat('assistant', '');
    currentAssistantContent = '';
  }
  
  currentAssistantContent += content;
  const bubble = currentAssistantMessage.querySelector('.message-bubble');
  bubble.innerHTML = SimpleMarkdown.parse(currentAssistantContent);
  scrollToBottom();
}

function updateLastAssistantMessageStats(stats) {
  if (!currentAssistantMessage) return;
  
  const statsEl = currentAssistantMessage.querySelector('.message-stats');
  if (statsEl && stats) {
    statsEl.innerHTML = `
      <div class="stat-item">
        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <circle cx="12" cy="12" r="10"/>
          <polyline points="12 6 12 12 16 14"/>
        </svg>
        <span>TTFT: ${stats.timeToFirstToken}ms</span>
      </div>
      <div class="stat-item">
        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>
        </svg>
        <span>${stats.tokensPerSecond} tok/s</span>
      </div>
      <div class="stat-item">
        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M22 11.08V12a10 10 0 11-5.93-9.14"/>
          <polyline points="22 4 12 14.01 9 11.01"/>
        </svg>
        <span>${stats.tokenCount} tokens</span>
      </div>
    `;
  }
  
  // Reset for next message
  currentAssistantMessage = null;
  currentAssistantContent = '';
}

function clearChat() {
  messages = [];
  currentAssistantMessage = null;
  currentAssistantContent = '';
  updateContextUsage();
  
  chatMessages.innerHTML = `
    <div class="welcome-message">
      <div class="welcome-icon">
        <svg width="48" height="48" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
          <path d="M21 15a2 2 0 01-2 2H7l-4 4V5a2 2 0 012-2h14a2 2 0 012 2v10z"/>
        </svg>
      </div>
      <h2>Welcome to Foundry Local Chat</h2>
      <p>Select a model from the sidebar to start chatting with AI running locally on your machine.</p>
      <div class="feature-highlights">
        <div class="feature">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
          </svg>
          <span>100% Private</span>
        </div>
        <div class="feature">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="10"/>
            <polyline points="12 6 12 12 16 14"/>
          </svg>
          <span>Low Latency</span>
        </div>
        <div class="feature">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <rect x="2" y="3" width="20" height="14" rx="2" ry="2"/>
            <line x1="8" y1="21" x2="16" y2="21"/>
            <line x1="12" y1="17" x2="12" y2="21"/>
          </svg>
          <span>Runs Locally</span>
        </div>
      </div>
    </div>
  `;
}

function scrollToBottom() {
  chatMessages.scrollTop = chatMessages.scrollHeight;
}

// Toast Notifications
function showToast(message, type = 'info') {
  const toast = document.createElement('div');
  toast.className = `toast ${type}`;
  toast.innerHTML = `
    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      ${type === 'success' ? '<polyline points="20 6 9 17 4 12"/>' :
        type === 'error' ? '<circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/>' :
        type === 'warning' ? '<path d="M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>' :
        '<circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/>'
      }
    </svg>
    <span>${message}</span>
  `;
  
  toastContainer.appendChild(toast);
  
  setTimeout(() => {
    toast.style.animation = 'slideIn 0.3s ease reverse';
    setTimeout(() => toast.remove(), 300);
  }, 3000);
}
