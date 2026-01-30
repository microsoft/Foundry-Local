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
      const language = lang || 'plaintext';
      return `<div class="code-block-wrapper">
        <div class="code-block-header">
          <span>${language}</span>
          <button class="copy-btn" onclick="copyCode(this)">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <rect x="9" y="9" width="13" height="13" rx="2" ry="2"/>
              <path d="M5 15H4a2 2 0 01-2-2V4a2 2 0 012-2h9a2 2 0 012 2v1"/>
            </svg>
            Copy
          </button>
        </div>
        <pre><code class="language-${language}">${code.trim()}</code></pre>
      </div>`;
    });
    
    // Inline code
    html = html.replace(/`([^`]+)`/g, '<code>$1</code>');
    
    // Bold
    html = html.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
    
    // Italic
    html = html.replace(/\*([^*]+)\*/g, '<em>$1</em>');
    
    // Links
    html = html.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2" target="_blank">$1</a>');
    
    // Line breaks
    html = html.replace(/\n/g, '<br>');
    
    return html;
  },
  
  escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }
};

// Copy code to clipboard
window.copyCode = async function(button) {
  const codeBlock = button.closest('.code-block-wrapper').querySelector('code');
  const text = codeBlock.textContent;
  
  try {
    await navigator.clipboard.writeText(text);
    button.innerHTML = `
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <polyline points="20 6 9 17 4 12"/>
      </svg>
      Copied!
    `;
    button.classList.add('copied');
    
    setTimeout(() => {
      button.innerHTML = `
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <rect x="9" y="9" width="13" height="13" rx="2" ry="2"/>
          <path d="M5 15H4a2 2 0 01-2-2V4a2 2 0 012-2h9a2 2 0 012 2v1"/>
        </svg>
        Copy
      `;
      button.classList.remove('copied');
    }, 2000);
  } catch (err) {
    console.error('Failed to copy:', err);
  }
};

// State
let messages = [];
let currentModelAlias = null;
let isGenerating = false;

// DOM Elements
const sidebar = document.getElementById('sidebar');
const sidebarToggle = document.getElementById('sidebarToggle');
const mobileMenuBtn = document.getElementById('mobileMenuBtn');
const modelList = document.getElementById('modelList');
const refreshModels = document.getElementById('refreshModels');
const currentModelEl = document.getElementById('currentModel');
const modelBadge = document.getElementById('modelBadge');
const chatMessages = document.getElementById('chatMessages');
const chatForm = document.getElementById('chatForm');
const messageInput = document.getElementById('messageInput');
const sendBtn = document.getElementById('sendBtn');
const newChatBtn = document.getElementById('newChatBtn');
const toastContainer = document.getElementById('toastContainer');

// Initialize
document.addEventListener('DOMContentLoaded', async () => {
  setupEventListeners();
  setupSidebarResize();
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
  if (model.alias === currentModelAlias) {
    item.classList.add('active');
  }
  
  const sizeMb = variant?.fileSizeMb;
  const sizeStr = sizeMb ? `${(sizeMb / 1024).toFixed(1)} GB` : '';
  
  item.innerHTML = `
    <div class="model-icon">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <path d="M21 16V8a2 2 0 00-1-1.73l-7-4a2 2 0 00-2 0l-7 4A2 2 0 003 8v8a2 2 0 001 1.73l7 4a2 2 0 002 0l7-4A2 2 0 0021 16z"/>
        <polyline points="3.27 6.96 12 12.01 20.73 6.96"/>
        <line x1="12" y1="22.08" x2="12" y2="12"/>
      </svg>
    </div>
    <div class="model-info">
      <div class="model-name">${variant?.displayName || model.alias}</div>
      <div class="model-size">${sizeStr}</div>
    </div>
    <div class="model-status">
      ${model.isCached 
        ? '<div class="status-indicator cached" title="Downloaded"></div>'
        : '<button class="download-btn">Download</button>'
      }
    </div>
  `;
  
  // Handle click to load model
  if (model.isCached) {
    item.addEventListener('click', () => loadModel(model.alias));
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

function updateCurrentModelDisplay(alias) {
  currentModelEl.innerHTML = `
    <span class="label">Active:</span>
    <span class="model-name">${alias}</span>
  `;
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
