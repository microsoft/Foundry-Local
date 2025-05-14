const { contextBridge, ipcRenderer } = require('electron');

console.log('Preload script starting...');
console.log('Current directory:', __dirname);
console.log('Module paths:', module.paths);

try {
    console.log('Electron modules loaded');
   
    contextBridge.exposeInMainWorld('versions', {
        node: () => process.versions.node,
        chrome: () => process.versions.chrome,
        electron: () => process.versions.electron
    })

    contextBridge.exposeInMainWorld('mainAPI', {
        initialize: () => ipcRenderer.invoke('initialize-client'),
        sendMessage: (messages) => ipcRenderer.invoke('send-message', messages),
        onChatChunk: (callback) => ipcRenderer.on('chat-chunk', (_, chunk) => callback(chunk)),
        onChatComplete: (callback) => ipcRenderer.on('chat-complete', () => callback()),
        removeAllChatListeners: () => {
            ipcRenderer.removeAllListeners('chat-chunk');
            ipcRenderer.removeAllListeners('chat-complete');
        }
    })

    console.log('Preload script completed successfully');
} catch (error) {
    console.error('Error in preload script:', error);
}