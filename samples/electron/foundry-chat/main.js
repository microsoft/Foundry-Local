import { app, BrowserWindow, Menu, ipcMain } from 'electron'
import { cloudApiKey } from './private.js'
import { fileURLToPath } from 'url'
import path from 'path'
import OpenAI from 'openai'
import { FoundryLocalManager } from 'foundry-local-sdk'


// Global variables
let mainWindow
let aiClient = null
const apiKey = cloudApiKey
let endpoint = null
let modelName = null
let foundryManager = null

//
// Phi-4 Azure AI Instance
//

// Read the host environment block for FOUNDRY_USE_CLOUD, and if non-zero, use the cloud endpoint
if (process.env.FOUNDRY_USE_CLOUD) {
  console.log("Using cloud endpoint")
  if (process.env.FOUNDRY_CLOUD_API_KEY) {
    apiKey = process.env.FOUNDRY_CLOUD_API_KEY
  }
  moelName = "Phi-4"
  endpoint = "https://foundry-chat-Phi-4.eastus2.models.ai.azure.com"
} else {
  console.log("Using local endpoint...")
  foundryManager = new FoundryLocalManager()
  modelName = (await foundryManager.init("phi-4-mini")).id;
  endpoint = foundryManager.endpoint
  apiKey = foundryManager.apiKey
  console.log("connected to local endpoint: " + endpoint)
}

// Simplified IPC handlers
ipcMain.handle('initialize-client', (_) => {
    return initializeClient()
})

ipcMain.handle('send-message', (_, messages) => {
  return sendMessage(messages)
})

// Initialize foundry client
export function initializeClient() {
  try {
      // Get AI Client ready for completions
      aiClient = new OpenAI({
        apiKey: apiKey,
        baseURL: endpoint
      })
      console.log("Client initialized successfully")

      return { 
          success: true,
          endpoint: endpoint,
          modelName: modelName
      }
  } catch (error) {
      return { 
          success: false, 
          error: error.message 
      }
  }
}

export async function sendMessage(messages) {
  try {
      if (!aiClient) {
          throw new Error('Client not initialized')
      }

      const stream = await aiClient.chat.completions.create({
        model: modelName,
        messages: messages,
        max_tokens: 1024,
        stream: true
      })

      for await (const chunk of stream) {
        const content = chunk.choices[0]?.delta?.content
        if (content) {
          mainWindow.webContents.send('chat-chunk', content)
        }
      }
      
      mainWindow.webContents.send('chat-complete')
      return { success: true }
  } catch (error) {
      return { success: false, error: error.message }
  }
} 

// Window management
async function createWindow() {
  // Dynamically import the preload script
  const __filename = fileURLToPath(import.meta.url)
  const __dirname = path.dirname(__filename)
  const preloadPath = path.join(__dirname, 'preload.cjs')
  
  mainWindow = new BrowserWindow({
    width: 1024,
    height: 768,
    autoHideMenuBar: false,
    webPreferences: {
      allowRunningInsecureContent: true,
      nodeIntegration: false,
      contextIsolation: true,
      preload: preloadPath,
      enableRemoteModule: false,
      sandbox: false
    }
  })

  Menu.setApplicationMenu(null)

  console.log("Creating chat window")
  mainWindow.loadFile('chat.html')
  
  // Send initial config to renderer
  mainWindow.webContents.on('did-finish-load', () => {
  })

  return mainWindow
}

// App lifecycle handlers
app.whenReady().then(() => {
  createWindow()

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow()
    }
  })
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit()
  }
})
