// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import * as client from './client.js'
import { ExecutionProvider } from './types.js'
import type { DownloadBody, Fetch, FoundryModelInfo, FoundryListResponseModel } from './types.js'

/**
 * Utility function to detect if the platform is Windows.
 * @returns {boolean} True if the platform is Windows, otherwise false.
 */
function isWindowsPlatform(): boolean {
  if (typeof process !== 'undefined' && process.platform) {
    return process.platform === 'win32' // Node.js context
  }
  if (typeof navigator !== 'undefined' && navigator.platform) {
    return navigator.platform.startsWith('Win') // Browser context
  }
  return false // Default to non-Windows
}

/**
 * Class representing the Foundry Local Manager compatible with browser environments.
 * This class needs to be constructed with a service URL and cannot manage the service itself.
 */
export class FoundryLocalManager {
  /**
   * The service URL for the Foundry service.
   */
  protected _serviceUrl: string | null

  /**
   * The fetch function to use for HTTP requests.
   */
  protected readonly fetch: Fetch

  /**
   * Cached list of catalog models.
   */
  protected catalogList: FoundryModelInfo[] | null = null

  /**
   * Cached record of catalog models.
   */
  protected catalogRecord: Record<string, FoundryModelInfo> | null = null

  /**
   * Constructs a new FoundryLocalManager instance.
   * @param {Object} options - Configuration options for the FoundryLocalManager.
   * @param {string} options.serviceUrl - The base URL of the Foundry service.
   * @param {Fetch} [options.fetch] - Optional custom fetch implementation to use for HTTP requests.
   * If not provided, the global fetch will be used.
   */
  constructor({ serviceUrl, fetch: overriddenFetch = fetch }: { serviceUrl: string; fetch?: Fetch }) {
    this._serviceUrl = serviceUrl
    this.fetch = overriddenFetch
  }

  /**
   * Gets the service URL.
   * @throws {Error} If the service URL is invalid.
   * @returns {string} The service URL.
   */
  get serviceUrl(): string {
    if (this._serviceUrl) {
      return this._serviceUrl
    }
    throw new Error('Service URL is invalid!')
  }

  /**
   * Gets the API endpoint URL.
   * @returns {string} The API endpoint URL.
   */
  get endpoint(): string {
    return `${this.serviceUrl}/v1`
  }

  /**
   * Gets the API key.
   * @returns {string} The API key.
   */
  get apiKey(): string {
    return 'OPENAI_API_KEY'
  }

  /**
   * Lists the catalog models.
   * @returns {Promise<FoundryModelInfo[]>} The list of catalog models.
   */
  async listCatalogModels(): Promise<FoundryModelInfo[]> {
    if (this.catalogList) {
      return this.catalogList
    }
    const response = await client.get(this.fetch, `${this.serviceUrl}/foundry/list`)
    const data = (await response.json()) as FoundryListResponseModel[]

    // List should provide the model info in the future
    this.catalogList = data.map((model) => ({
      alias: model.alias,
      id: model.name,
      version: model.version,
      runtime: model.runtime.executionProvider,
      uri: model.uri,
      modelSize: model.fileSizeMb,
      promptTemplate: model.promptTemplate,
      provider: model.providerType,
      publisher: model.publisher,
      license: model.license,
      task: model.task,
    }))
    return this.catalogList
  }

  /**
   * Gets the catalog record.
   * @returns {Promise<Record<string, FoundryModelInfo>>} The catalog record.
   */
  private async getCatalogRecord(): Promise<Record<string, FoundryModelInfo>> {
    if (this.catalogRecord) {
      return this.catalogRecord
    }

    const catalogList = await this.listCatalogModels()
    const aliasCandidates: Record<string, FoundryModelInfo[]> = {}

    // Build the catalog record and alias candidates
    this.catalogRecord = catalogList.reduce(
      (acc, model) => {
        acc[model.id] = model
        ;(aliasCandidates[model.alias] ||= []).push(model)
        return acc
      },
      {} as Record<string, FoundryModelInfo>,
    )

    // Define the preferred order of execution providers
    const preferredOrder = [
      ExecutionProvider.QNN,
      ExecutionProvider.CUDA,
      ...(isWindowsPlatform()
        ? [ExecutionProvider.CPU, ExecutionProvider.WEBGPU]
        : [ExecutionProvider.WEBGPU, ExecutionProvider.CPU]),
    ]

    // Create a priority map for efficient sorting
    const priorityMap = new Map(preferredOrder.map((provider, index) => [provider, index]))

    // Choose the preferred model for each alias
    Object.entries(aliasCandidates).forEach(([alias, candidates]) => {
      const bestCandidate = candidates.reduce((best, current) => {
        const bestPriority = priorityMap.get(best.runtime) ?? Infinity
        const currentPriority = priorityMap.get(current.runtime) ?? Infinity
        return currentPriority < bestPriority ? current : best
      })

      // Explicitly assign the best candidate to avoid null/undefined issues
      if (this.catalogRecord) {
        this.catalogRecord[alias] = bestCandidate
      }
    })

    return this.catalogRecord
  }

  /**
   * Refreshes the catalog by clearing cached data.
   * @returns {Promise<void>} Resolves when the catalog is refreshed.
   */
  async refreshCatalog(): Promise<void> {
    this.catalogList = null
    this.catalogRecord = null
  }

  /**
   * Gets model information by alias or ID.
   * @param {string} aliasOrModelId - The alias or model ID.
   * @param {boolean} throwOnNotFound - Whether to throw an error if the model is not found.
   * @returns {Promise<FoundryModelInfo | null>} The model information or null if not found.
   */
  async getModelInfo(aliasOrModelId: string, throwOnNotFound = false): Promise<FoundryModelInfo | null> {
    const catalogRecord = await this.getCatalogRecord()
    const modelInfo = catalogRecord[aliasOrModelId]
    if (!modelInfo && throwOnNotFound) {
      throw new Error(`Model with alias or ID ${aliasOrModelId} not found in the catalog`)
    }
    return modelInfo ?? null
  }

  /**
   * Fetches models based on a provided endpoint.
   * @param {string} path - The endpoint path.
   * @returns {Promise<FoundryModelInfo[]>} The list of models.
   */
  private async fetchModels(path: string): Promise<FoundryModelInfo[]> {
    const response = await client.get(this.fetch, `${this.serviceUrl}${path}`)
    const modelNames: string[] = await response.json()

    // List should provide the model info in the future
    const models = await Promise.all(modelNames.map(async (name) => this.getModelInfo(name)))

    return models.filter((model): model is FoundryModelInfo => model !== null)
  }

  /**
   * Gets the cache location.
   * @returns {Promise<string>} The cache location.
   */
  async getCacheLocation(): Promise<string> {
    const response = await client.get(this.fetch, `${this.serviceUrl}/openai/status`)
    const data = await response.json()
    return data.modelDirPath
  }

  /**
   * Lists cached models.
   * @returns {Promise<FoundryModelInfo[]>} The list of models downloaded to the cache.
   */
  async listCachedModels(): Promise<FoundryModelInfo[]> {
    return await this.fetchModels('/openai/models')
  }

  /**
   * Downloads a model.
   * @param {string} aliasOrModelId - The alias or model ID.
   * @param {string} [token] - Optional token for authentication.
   * @param {boolean} force - Whether to force download an already downloaded model.
   * @param {(progress: number) => void} [onProgress] - Callback for download progress percentage.
   * If model is already downloaded and force is false, it will be called once with 100.
   * @returns {Promise<FoundryModelInfo>} The downloaded model information.
   */
  async downloadModel(
    aliasOrModelId: string,
    token?: string,
    force = false,
    onProgress?: (progress: number) => void,
  ): Promise<FoundryModelInfo> {
    const modelInfo = (await this.getModelInfo(aliasOrModelId, true)) as FoundryModelInfo

    const cachedModels = await this.listCachedModels()
    if (cachedModels.some((model) => model.id === modelInfo.id)) {
      if (!force) {
        if (onProgress) {
          onProgress(100) // Report 100% progress since model is already downloaded
        }
        // Model already downloaded. Skipping download.
        return modelInfo
      }
    }

    const downloadBody: DownloadBody = {
      Name: modelInfo.id,
      Uri: modelInfo.uri,
      Publisher: modelInfo.publisher,
      ProviderType: modelInfo.provider === 'AzureFoundry' ? `${modelInfo.provider}Local` : modelInfo.provider,
      PromptTemplate: modelInfo.promptTemplate,
    }
    const body = {
      model: downloadBody,
      ...(token && { token }),
      IgnorePipeReport: true,
    }

    const data = await client.postWithProgress(this.fetch, `${this.serviceUrl}/openai/download`, body, onProgress)

    if (!data.success) {
      throw new Error(
        `Failed to download model with alias '${modelInfo.alias}' and ID '${modelInfo.id}': ${data.error}`,
      )
    }

    return modelInfo
  }
  /**
   * Loads a model.
   * @param {string} aliasOrModelId - The alias or model ID.
   * @param {number} ttl - Time-to-live for the model in seconds. Default is 600 seconds (10 minutes).
   * @returns {Promise<FoundryModelInfo>} The loaded model information.
   * @throws {Error} If the model is not in the catalog or has not been downloaded yet.
   */
  async loadModel(aliasOrModelId: string, ttl = 600): Promise<FoundryModelInfo> {
    const modelInfo = (await this.getModelInfo(aliasOrModelId, true)) as FoundryModelInfo

    const queryParams: Record<string, string> = { ttl: ttl.toString() }
    if (modelInfo.runtime === ExecutionProvider.WEBGPU || modelInfo.runtime === ExecutionProvider.CUDA) {
      // These models might have empty ep or dml ep in the genai config
      // Use cuda if available, otherwise use the model's runtime
      const catalogModel = await this.listCatalogModels()
      const hasCudaSupport = catalogModel.some((mi) => mi.runtime === ExecutionProvider.CUDA)

      queryParams['ep'] = hasCudaSupport
        ? ExecutionProvider.CUDA.replace('ExecutionProvider', '').toLowerCase()
        : modelInfo.runtime.replace('ExecutionProvider', '').toLowerCase()
    }

    try {
      await client.get(this.fetch, `${this.serviceUrl}/openai/load/${modelInfo.id}`, queryParams)
    } catch (error) {
      if (error instanceof Error && error.message.includes('No OpenAIService provider found for modelName')) {
        throw new Error(`Model ${aliasOrModelId} has not been downloaded yet. Please download it first.`)
      }
      throw error
    }

    return modelInfo
  }

  /**
   * Unloads a model.
   * @param {string} aliasOrModelId - The alias or model ID.
   * @param {boolean} force - Whether to force unload model with TTL.
   * @returns {Promise<void>} Resolves when the model is unloaded.
   */
  async unloadModel(aliasOrModelId: string, force = false): Promise<void> {
    const modelInfo = (await this.getModelInfo(aliasOrModelId, true)) as FoundryModelInfo

    const loadedModels = await this.listLoadedModels()
    if (!loadedModels.some((model) => model.id === modelInfo.id)) {
      return
    }

    await client.get(this.fetch, `${this.serviceUrl}/openai/unload/${modelInfo.id}`, {
      force: force.toString(),
    })
  }

  /**
   * Lists loaded models.
   * @returns {Promise<FoundryModelInfo[]>} The list of loaded models.
   */
  async listLoadedModels(): Promise<FoundryModelInfo[]> {
    return await this.fetchModels('/openai/loadedmodels')
  }
}

export * from './types.js'
