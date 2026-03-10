const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('foundryAPI', {
  getModels: () => ipcRenderer.invoke('get-models'),
  downloadModel: (modelAlias) => ipcRenderer.invoke('download-model', modelAlias),
  loadModel: (modelAlias) => ipcRenderer.invoke('load-model', modelAlias),
  unloadModel: () => ipcRenderer.invoke('unload-model'),
  deleteModel: (modelAlias) => ipcRenderer.invoke('delete-model', modelAlias),
  chat: (messages) => ipcRenderer.invoke('chat', messages),
  getLoadedModel: () => ipcRenderer.invoke('get-loaded-model'),
  onChatChunk: (callback) => {
    const handler = (event, data) => callback(data);
    ipcRenderer.on('chat-chunk', handler);
    return () => ipcRenderer.removeListener('chat-chunk', handler);
  },
  // Transcription
  getWhisperModels: () => ipcRenderer.invoke('get-whisper-models'),
  downloadWhisperModel: (modelAlias) => ipcRenderer.invoke('download-whisper-model', modelAlias),
  transcribeAudio: (filePath, base64Data) => ipcRenderer.invoke('transcribe-audio', filePath, base64Data)
});
