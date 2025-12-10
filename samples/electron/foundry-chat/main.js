import { app, BrowserWindow, Menu, ipcMain } from 'electron'
import { fileURLToPath } from 'url'
import path from 'path'
import OpenAI from 'openai'
import { FoundryLocalManager } from 'foundry-local-sdk'


// Global variables
let mainWindow
let aiClient = null
let currentModelType = 'cloud' // Add this to track current model type, default to cloud
let modelName = null
let endpoint = null
let apiKey = ""

const cloudApiKey = process.env.YOUR_API_KEY // load cloud api key from environment variable
const cloudEndpoint = process.env.YOUR_ENDPOINT // load cloud endpoint from environment variable
const cloudModelName = process.env.YOUR_MODEL_NAME // load cloud model name from environment variable
// Check if all required environment variables are set
if (!cloudApiKey || !cloudEndpoint || !cloudModelName) {
  console.error('Cloud API key, endpoint, or model name not set in environment variables, cloud mode will not work')
  console.error('Please set YOUR_API_KEY, YOUR_ENDPOINT, and YOUR_MODEL_NAME')
}

// Create and initialize the FoundryLocalManager and start the service
const foundryManager = new FoundryLocalManager()
if (!foundryManager.isServiceRunning()) {
    console.error('Foundry Local service is not running')
    app.quit()
}

// Simplified IPC handlers
ipcMain.handle('send-message', (_, messages) => {
  return sendMessage(messages)
})

// Add new IPC handler for getting local models
ipcMain.handle('get-local-models', async () => {
  if (!foundryManager) {
    return { success: false, error: 'Local manager not initialized' }
  }
  try {
    const models = await foundryManager.listCachedModels()
    return { success: true, models }
  } catch (error) {
    return { success: false, error: error.message }
  }
})

// Add new IPC handler for switching models
ipcMain.handle('switch-model', async (_, modelId) => {
  try {
    if (modelId === 'cloud') {
      console.log("Switching to cloud model")
      currentModelType = 'cloud'
      endpoint = cloudEndpoint
      apiKey = cloudApiKey
      modelName = cloudModelName
    } else {
      console.log("Switching to local model")
      currentModelType = 'local'
      modelName = (await foundryManager.init(modelId)).id
      endpoint = foundryManager.endpoint
      apiKey = foundryManager.apiKey
    }

    aiClient = new OpenAI({
      apiKey: apiKey,
      baseURL: endpoint
    })

    return { 
      success: true,
      endpoint: endpoint,
      modelName: modelName
    }
  } catch (error) {
    return { success: false, error: error.message }
  }
})

export async function sendMessage(messages) {
  try {
      if (!aiClient) {
          throw new Error('Client not initialized')
      }

      const stream = await aiClient.chat.completions.create({
        model: modelName,
        messages: messages,
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
    // Initialize with cloud model after page loads
    mainWindow.webContents.send('initialize-with-cloud')
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
