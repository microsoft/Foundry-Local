const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('foundryAPI', {
  getModels: () => ipcRenderer.invoke('get-models'),
  downloadModel: (modelAlias) => ipcRenderer.invoke('download-model', modelAlias),
  loadModel: (modelAlias) => ipcRenderer.invoke('load-model', modelAlias),
  unloadModel: () => ipcRenderer.invoke('unload-model'),
  chat: (messages) => ipcRenderer.invoke('chat', messages),
  getLoadedModel: () => ipcRenderer.invoke('get-loaded-model'),
  onChatChunk: (callback) => {
    ipcRenderer.on('chat-chunk', (event, data) => callback(data));
    return () => ipcRenderer.removeAllListeners('chat-chunk');
  }
});
