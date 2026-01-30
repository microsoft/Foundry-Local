const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    minWidth: 800,
    minHeight: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false
    },
    titleBarStyle: 'hiddenInset',
    backgroundColor: '#1a1a2e'
  });

  mainWindow.loadFile('index.html');
  
  // Open DevTools in development
  if (process.argv.includes('--enable-logging')) {
    mainWindow.webContents.openDevTools();
  }
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow();
  }
});

// SDK Management
let manager = null;
let currentModel = null;
let chatClient = null;
let webServiceStarted = false;
const SERVICE_PORT = 47392;
const SERVICE_URL = `http://127.0.0.1:${SERVICE_PORT}`;

async function initializeSDK() {
  if (manager) return manager;
  
  const { FoundryLocalManager } = await import('@prathikrao/foundry-local-sdk');
  manager = FoundryLocalManager.create({
    appName: 'foundry_local_electron_chat',
    logLevel: 'info',
    webServiceUrls: SERVICE_URL
  });
  
  return manager;
}

function ensureWebServiceStarted() {
  if (!webServiceStarted && manager) {
    manager.startWebService();
    webServiceStarted = true;
  }
}

// IPC Handlers
ipcMain.handle('get-models', async () => {
  try {
    console.log('get-models: initializing SDK...');
    await initializeSDK();
    
    console.log('get-models: fetching models from catalog...');
    const models = await manager.catalog.getModels();
    console.log(`get-models: found ${models.length} models`);
    
    const cachedVariants = await manager.catalog.getCachedModels();
    const cachedIds = new Set(cachedVariants.map(v => v.id));
    console.log(`get-models: ${cachedVariants.length} cached models`);
    
    const result = models.map(m => ({
      id: m.id,
      alias: m.alias,
      isCached: m.isCached,
      variants: m.variants.map(v => ({
        id: v.id,
        alias: v.alias,
        displayName: v.modelInfo.displayName || v.alias,
        isCached: cachedIds.has(v.id),
        fileSizeMb: v.modelInfo.fileSizeMb,
        modelType: v.modelInfo.modelType,
        publisher: v.modelInfo.publisher
      }))
    }));
    
    console.log('get-models: returning', result.length, 'models');
    return result;
  } catch (error) {
    console.error('Error getting models:', error);
    throw error;
  }
});

ipcMain.handle('download-model', async (event, modelAlias) => {
  try {
    await initializeSDK();
    const model = await manager.catalog.getModel(modelAlias);
    if (!model) throw new Error(`Model ${modelAlias} not found`);
    
    model.download();
    return { success: true };
  } catch (error) {
    console.error('Error downloading model:', error);
    throw error;
  }
});

ipcMain.handle('load-model', async (event, modelAlias) => {
  try {
    await initializeSDK();
    
    // Start web service for HTTP streaming (only once)
    ensureWebServiceStarted();
    
    // Unload current model if any
    if (currentModel) {
      try {
        await currentModel.unload();
      } catch (e) {
        // Ignore unload errors
      }
      chatClient = null;
    }
    
    const model = await manager.catalog.getModel(modelAlias);
    if (!model) throw new Error(`Model ${modelAlias} not found`);
    
    // Download if not cached
    if (!model.isCached) {
      model.download();
    }
    
    await model.load();
    
    // Wait for model to be fully loaded before creating chat client
    while (!(await model.isLoaded())) {
      await new Promise(resolve => setTimeout(resolve, 100));
    }
    
    currentModel = model;
    chatClient = model.createChatClient();
    
    return { success: true, modelId: model.id };
  } catch (error) {
    console.error('Error loading model:', error);
    throw error;
  }
});

ipcMain.handle('unload-model', async () => {
  try {
    if (currentModel) {
      await currentModel.unload();
      currentModel = null;
      chatClient = null;
    }
    return { success: true };
  } catch (error) {
    console.error('Error unloading model:', error);
    throw error;
  }
});

ipcMain.handle('chat', async (event, messages) => {
  if (!currentModel) throw new Error('No model loaded');
  
  const startTime = performance.now();
  let firstTokenTime = null;
  let tokenCount = 0;
  let fullContent = '';
  
  // Use HTTP streaming to avoid koffi callback issues with Electron
  const response = await fetch('http://localhost:47392/v1/chat/completions', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      model: currentModel.id,
      messages,
      stream: true
    })
  });
  
  if (!response.ok) {
    throw new Error(`HTTP error: ${response.status}`);
  }
  
  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  
  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    
    const chunk = decoder.decode(value, { stream: true });
    const lines = chunk.split('\n').filter(line => line.startsWith('data: '));
    
    for (const line of lines) {
      const data = line.slice(6); // Remove 'data: ' prefix
      if (data === '[DONE]') continue;
      
      try {
        const parsed = JSON.parse(data);
        const content = parsed.choices?.[0]?.delta?.content;
        if (content) {
          if (firstTokenTime === null) {
            firstTokenTime = performance.now();
          }
          tokenCount++;
          fullContent += content;
          
          mainWindow.webContents.send('chat-chunk', {
            content,
            tokenCount,
            timeToFirstToken: firstTokenTime ? (firstTokenTime - startTime) : null
          });
        }
      } catch (e) {
        // Skip invalid JSON chunks
      }
    }
  }
  
  const endTime = performance.now();
  const totalTime = endTime - startTime;
  const tokensPerSecond = tokenCount > 0 ? (tokenCount / (totalTime / 1000)).toFixed(2) : 0;
  
  return {
    content: fullContent,
    stats: {
      tokenCount,
      timeToFirstToken: firstTokenTime ? Math.round(firstTokenTime - startTime) : 0,
      totalTime: Math.round(totalTime),
      tokensPerSecond: parseFloat(tokensPerSecond)
    }
  };
});

ipcMain.handle('get-loaded-model', async () => {
  if (!currentModel) return null;
  return {
    id: currentModel.id,
    alias: currentModel.alias
  };
});
