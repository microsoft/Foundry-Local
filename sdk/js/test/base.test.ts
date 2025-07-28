// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { describe, expect, it, vi, beforeEach } from 'vitest'
import { FoundryLocalManager } from '../src/base'
import * as client from '../src/client'
import { ExecutionProvider } from '../src/types'

describe('FoundryLocalManager', () => {
  let manager: FoundryLocalManager
  const mockFetch = vi.fn()

  // Mock client module
  vi.mock('../src/client', () => ({
    get: vi.fn(),
    postWithProgress: vi.fn(),
  }))

  beforeEach(() => {
    vi.resetAllMocks()
    // Setup instance with mock fetch
    manager = new FoundryLocalManager({
      serviceUrl: 'http://localhost:5273',
      fetch: mockFetch as any,
    })
  })

  describe('constructor', () => {
    it('should set serviceUrl and fetch', () => {
      expect(manager['_serviceUrl']).toBe('http://localhost:5273')
      expect(manager['fetch']).toBe(mockFetch)
    })
  })

  describe('serviceUrl', () => {
    it('should return the service URL', () => {
      expect(manager.serviceUrl).toBe('http://localhost:5273')
    })

    it('should throw error if service URL is not set', () => {
      manager['_serviceUrl'] = null
      expect(() => manager.serviceUrl).toThrow('Service URL is invalid')
    })
  })

  describe('endpoint', () => {
    it('should return the API endpoint URL', () => {
      expect(manager.endpoint).toBe('http://localhost:5273/v1')
    })
  })

  describe('apiKey', () => {
    it('should return the API key', () => {
      expect(manager.apiKey).toBe('OPENAI_API_KEY')
    })
  })

  describe('listCatalogModels', () => {
    it('should fetch and transform catalog models', async () => {
      const mockResponse = {
        json: vi.fn().mockResolvedValue([
          {
            name: 'model_name:1',
            displayName: 'model_name',
            modelType: 'ONNX',
            providerType: 'AzureFoundry',
            uri: 'azureml://registries/azureml/models/model_name/versions/1',
            version: '1',
            promptTemplate: { prompt: '<|start|>{Content}<|end|>' },
            publisher: 'Microsoft',
            task: 'chat-completion',
            runtime: { deviceType: 'CPU', executionProvider: 'CPUExecutionProvider' },
            fileSizeMb: 10403,
            modelSettings: { parameters: [] },
            alias: 'model_alias',
            supportsToolCalling: false,
            license: 'MIT',
            licenseDescription: 'This model is provided under the License Terms available at ...',
            parentModelUri: 'azureml://registries/azureml/models/model_parent/versions/1',
            maxOutputTokens: 1024,
            minFLVersion: '1.0.0',
          },
        ]),
      }

      vi.mocked(client.get).mockResolvedValue(mockResponse as any)

      const models = await manager.listCatalogModels()

      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/foundry/list')
      expect(models).toHaveLength(1)
      expect(models[0]).toEqual({
        alias: 'model_alias',
        id: 'model_name:1',
        version: '1',
        runtime: ExecutionProvider.CPU,
        uri: 'azureml://registries/azureml/models/model_name/versions/1',
        modelSize: 10403,
        promptTemplate: { prompt: '<|start|>{Content}<|end|>' },
        provider: 'AzureFoundry',
        publisher: 'Microsoft',
        license: 'MIT',
        task: 'chat-completion',
      })
    })

    it('should return cached catalog if available', async () => {
      // Setup mock catalog
      manager['catalogList'] = [
        {
          alias: 'model_alias',
          id: 'model_name:1',
          version: '1',
          runtime: ExecutionProvider.CPU,
          uri: 'azureml://registries/azureml/models/model_name/versions/1',
          modelSize: 10403,
          promptTemplate: { prompt: '<|start|>{Content}<|end|>' },
          provider: 'AzureFoundry',
          publisher: 'Microsoft',
          license: 'MIT',
          task: 'chat-completion',
        },
      ]

      const models = await manager.listCatalogModels()

      expect(client.get).not.toHaveBeenCalled()
      expect(models).toEqual(manager['catalogList'])
    })
  })

  describe('refreshCatalog', () => {
    it('should clear catalog cache', async () => {
      // Setup mock catalog
      manager['catalogList'] = [{ alias: 'model', id: 'id' } as any]
      manager['catalogRecord'] = { id: { alias: 'model', id: 'id' } as any }

      await manager.refreshCatalog()

      expect(manager['catalogList']).toBeNull()
      expect(manager['catalogRecord']).toBeNull()
    })
  })

  describe('getModelInfo', () => {
    beforeEach(async () => {
      // Setup mock catalog list
      manager['catalogList'] = [
        // eneric-gpu, generic-cpu
        {
          id: 'model-1-generic-gpu:1',
          runtime: ExecutionProvider.WEBGPU,
          alias: 'model-1',
        } as any,
        {
          id: 'model-1-generic-cpu:1',
          runtime: ExecutionProvider.CPU,
          alias: 'model-1',
        },
        {
          id: 'model-1-generic-cpu:2',
          runtime: ExecutionProvider.CPU,
          alias: 'model-1',
        },
        // npu, generic-cpu
        {
          id: 'model-2-npu:1',
          runtime: ExecutionProvider.QNN,
          alias: 'model-2',
        },
        {
          id: 'model-2-npu:2',
          runtime: ExecutionProvider.QNN,
          alias: 'model-2',
        },
        {
          id: 'model-2-generic-cpu:1',
          runtime: ExecutionProvider.CPU,
          alias: 'model-2',
        },
        // cuda-gpu, generic-gpu, generic-cpu
        {
          id: 'model-3-cuda-gpu:1',
          runtime: ExecutionProvider.CUDA,
          alias: 'model-3',
        },
        {
          id: 'model-3-generic-gpu:1',
          runtime: ExecutionProvider.WEBGPU,
          alias: 'model-3',
        },
        {
          id: 'model-3-generic-cpu:1',
          runtime: ExecutionProvider.CPU,
          alias: 'model-3',
        },
        // generic-cpu
        {
          id: 'model-4-generic-cpu:1',
          runtime: ExecutionProvider.CPU,
          alias: 'model-4',
        },
      ]
    })

    it('should return model info by id', async () => {
      expect((await manager.getModelInfo('model-1-generic-gpu'))?.id).toBe('model-1-generic-gpu:1')
      expect((await manager.getModelInfo('model-1-generic-cpu'))?.id).toBe('model-1-generic-cpu:2')
    })

    it('should return model info by alias on Windows', async () => {
      vi.spyOn(process, 'platform', 'get').mockReturnValue('win32')

      expect((await manager.getModelInfo('model-1'))?.id).toBe('model-1-generic-cpu:2') // cpu is preferred over webgpu
      expect((await manager.getModelInfo('model-2'))?.id).toBe('model-2-npu:2') // npu most preferred
      expect((await manager.getModelInfo('model-3'))?.id).toBe('model-3-cuda-gpu:1') // cuda most preferred
      expect((await manager.getModelInfo('model-4'))?.id).toBe('model-4-generic-cpu:1') // generic-cpu
    })

    it('should return model info by alias on non-Windows', async () => {
      vi.spyOn(process, 'platform', 'get').mockReturnValue('linux')

      expect((await manager.getModelInfo('model-1'))?.id).toBe('model-1-generic-gpu:1') // webgpu is preferred over cpu
      expect((await manager.getModelInfo('model-2'))?.id).toBe('model-2-npu:2') // npu most preferred
      expect((await manager.getModelInfo('model-3'))?.id).toBe('model-3-cuda-gpu:1') // cuda most preferred
      expect((await manager.getModelInfo('model-4'))?.id).toBe('model-4-generic-cpu:1') // generic-cpu
    })

    it('should return null for non-existent model', async () => {
      const modelInfo = await manager.getModelInfo('non_existent')

      expect(modelInfo).toBeNull()
    })

    it('should throw error for non-existent model when throwOnNotFound is true', async () => {
      await expect(manager.getModelInfo('non_existent', true)).rejects.toThrow(
        'Model with alias or ID \'non_existent\' not found in the catalog',
      )
    })
  })

  describe('getCacheLocation', () => {
    it('should return cache location from status API', async () => {
      const mockResponse = {
        json: vi.fn().mockResolvedValue({
          modelDirPath: '/path/to/cache',
        }),
      }

      vi.mocked(client.get).mockResolvedValue(mockResponse as any)

      const location = await manager.getCacheLocation()

      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/status')
      expect(location).toBe('/path/to/cache')
    })
  })

  describe('listCachedModels', () => {
    it('should fetch and return local models', async () => {
      // Mock for fetchModels (which is private)
      const mockModelNames = ['model1', 'model2']
      const mockResponse = {
        json: vi.fn().mockResolvedValue(mockModelNames),
      }

      vi.mocked(client.get).mockResolvedValue(mockResponse as any)

      // Setup mock getModelInfo to return model info for each name
      vi.spyOn(manager, 'getModelInfo').mockImplementation(
        async (name) =>
          ({
            alias: `${name}_alias`,
            id: name,
            runtime: ExecutionProvider.CPU,
          }) as any,
      )

      const models = await manager.listCachedModels()

      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/models')
      expect(models).toHaveLength(2)
      expect(models[0]).toEqual({
        alias: 'model1_alias',
        id: 'model1',
        runtime: ExecutionProvider.CPU,
      })
    })
  })

  describe('downloadModel', () => {
    it('should skip download if model is already downloaded and force is false', async () => {
      // Setup model info
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.CPU,
        uri: 'https://example.com/model',
        publisher: 'Microsoft',
        provider: 'AzureFoundry',
        promptTemplate: {},
      } as any)

      // Setup listCachedModels to include the model
      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([{ id: 'model_id' } as any])

      const mockOnProgress = vi.fn()

      const result = await manager.downloadModel('model_alias', undefined, false, mockOnProgress)

      expect(client.postWithProgress).not.toHaveBeenCalled()
      expect(mockOnProgress).toHaveBeenCalledWith(100)
      expect(result).toEqual({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.CPU,
        uri: 'https://example.com/model',
        publisher: 'Microsoft',
        provider: 'AzureFoundry',
        promptTemplate: {},
      })
    })

    it('should download model if not already downloaded', async () => {
      // Setup model info
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.CPU,
        uri: 'https://example.com/model',
        publisher: 'Microsoft',
        provider: 'AzureFoundry',
        promptTemplate: {},
      } as any)

      // Setup listCachedModels to not include the model
      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([])

      // Setup postWithProgress to return success
      vi.mocked(client.postWithProgress).mockResolvedValue({ success: true } as any)

      const result = await manager.downloadModel('model_alias')

      expect(client.postWithProgress).toHaveBeenCalledWith(
        mockFetch,
        'http://localhost:5273/openai/download',
        {
          model: {
            Name: 'model_id',
            Uri: 'https://example.com/model',
            Publisher: 'Microsoft',
            ProviderType: 'AzureFoundryLocal',
            PromptTemplate: {},
          },
          token: undefined,
          IgnorePipeReport: true,
        },
        undefined,
      )

      expect(result).toEqual({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.CPU,
        uri: 'https://example.com/model',
        publisher: 'Microsoft',
        provider: 'AzureFoundry',
        promptTemplate: {},
      })
    })

    it('should throw error if download fails', async () => {
      // Setup model info
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.CPU,
        uri: 'https://example.com/model',
        publisher: 'Microsoft',
        provider: 'AzureFoundry',
        promptTemplate: {},
      } as any)

      // Setup listCachedModels to not include the model
      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([])

      // Setup postWithProgress to return failure
      vi.mocked(client.postWithProgress).mockResolvedValue({
        success: false,
        error: 'Download failed',
      } as any)

      await expect(manager.downloadModel('model_alias')).rejects.toThrow(
        "Failed to download model with alias 'model_alias' and ID 'model_id': Download failed",
      )
    })
  })

  describe('isModelUpgradeable', () => {
    it('returns true if model is not cached', async () => {
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        id: 'model-3-cuda-gpu:1',
        alias: 'model-3',
      } as any)

      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([])

      vi.spyOn(manager, 'listCatalogModels').mockResolvedValue([
        {
          id: 'model-3-cuda-gpu:1',
          alias: 'model-3',
          runtime: 'CUDAExecutionProvider',
        } as any,
      ])

      const result = await manager.isModelUpgradeable('model-3')
      expect(result).toBe(true)
    })

    it('returns true if model is cached but older version', async () => {
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        id: 'model-2-npu:2',
        alias: 'model-2',
        runtime: ExecutionProvider.QNN,
      } as any)

      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([
        { id: 'model-2-npu:1' } as any,
      ])

      vi.spyOn(manager, 'listCatalogModels').mockResolvedValue([
        {
          id: 'model-2-npu:2',
          alias: 'model-2',
          runtime: ExecutionProvider.QNN,
        } as any,
      ])

      const result = await manager.isModelUpgradeable('model-2-npu:1')
      expect(result).toBe(true)
    })

    it('returns false if model is cached and latest version', async () => {
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        id: 'model-4-generic-gpu:1',
        alias: 'model-4',
        runtime: ExecutionProvider.WEBGPU,
      } as any)

      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([
        { id: 'model-4-generic-gpu:1' } as any,
      ])

      vi.spyOn(manager, 'listCatalogModels').mockResolvedValue([
        {
          id: 'model-4-generic-gpu:1',
          alias: 'model-4',
          runtime: ExecutionProvider.WEBGPU,
        } as any,
      ])

      const result = await manager.isModelUpgradeable('model-4')
      expect(result).toBe(false)
    })

    it('returns false if model version is invalid', async () => {
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        id: 'model-invalid-version',
        alias: 'model-invalid',
        runtime: ExecutionProvider.CPU,
      } as any)

      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([])

      // simulate getVersion returning -1
      vi.spyOn(manager as any, 'getVersion').mockReturnValue(-1)

      vi.spyOn(manager, 'listCatalogModels').mockResolvedValue([
        {
          id: 'model-invalid-version',
          alias: 'model-invalid',
          runtime: ExecutionProvider.CPU,
        } as any,
      ])

      const result = await manager.isModelUpgradeable('model-invalid-version')
      expect(result).toBe(false)
    })
  })

  describe('upgradeModel', () => {
    it('downloads model if not in cache', async () => {
      const mockModel = {
        id: 'model-3-cuda-gpu:1',
        alias: 'model-3',
        runtime: ExecutionProvider.CUDA,
        uri: 'https://example.com/model',
        publisher: 'Microsoft',
        provider: 'AzureFoundry',
        promptTemplate: {},
      } as any

      vi.spyOn(manager, 'getLatestModelInfo').mockResolvedValue(mockModel)
      const downloadSpy = vi.spyOn(manager, 'downloadModel').mockResolvedValue(mockModel)

      const result = await manager.upgradeModel('model-3')

      expect(manager.getLatestModelInfo).toHaveBeenCalledWith('model-3', true)
      expect(downloadSpy).toHaveBeenCalledWith('model-3-cuda-gpu:1', undefined, false, undefined)
      expect(result).toEqual(mockModel)
    })

    it('downloads latest version if older version is in cache', async () => {
      const mockModel = {
        id: 'model-2-npu:2',
        alias: 'model-2',
        runtime: ExecutionProvider.QNN,
        uri: 'https://example.com/model2',
        publisher: 'Microsoft',
        provider: 'AzureFoundry',
        promptTemplate: {},
      } as any

      vi.spyOn(manager, 'getLatestModelInfo').mockResolvedValue(mockModel)
      const downloadSpy = vi.spyOn(manager, 'downloadModel').mockResolvedValue(mockModel)

      const result = await manager.upgradeModel('model-2-npu:1')

      expect(manager.getLatestModelInfo).toHaveBeenCalledWith('model-2-npu:1', true)
      expect(downloadSpy).toHaveBeenCalledWith('model-2-npu:2', undefined, false, undefined)
      expect(result).toEqual(mockModel)
    })

    it('does not redownload model if already latest', async () => {
      const mockModel = {
        id: 'model-4-generic-gpu:1',
        alias: 'model-4',
        runtime: ExecutionProvider.WEBGPU,
        uri: 'https://example.com/model4',
        publisher: 'Microsoft',
        provider: 'AzureFoundry',
        promptTemplate: {},
      } as any

      vi.spyOn(manager, 'getLatestModelInfo').mockResolvedValue(mockModel)
      const downloadSpy = vi.spyOn(manager, 'downloadModel').mockResolvedValue(mockModel)

      const result = await manager.upgradeModel('model-4')

      expect(manager.getLatestModelInfo).toHaveBeenCalledWith('model-4', true)
      expect(downloadSpy).toHaveBeenCalledWith('model-4-generic-gpu:1', undefined, false, undefined)
      expect(result).toEqual(mockModel)
    })

    it('throws error if getLatestModelInfo fails', async () => {
      vi.spyOn(manager, 'getLatestModelInfo').mockRejectedValue(new Error('Not found'))

      await expect(manager.upgradeModel('nonexistent-model')).rejects.toThrow('Not found')
    })
  })

  describe('loadModel', () => {
    it('should load model with default TTL', async () => {
      // Setup model info
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.CPU,
      } as any)

      const mockResponse = {}
      vi.mocked(client.get).mockResolvedValue(mockResponse as any)

      const result = await manager.loadModel('model_alias')

      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/load/model_id', { ttl: '600' })

      expect(result).toEqual({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.CPU,
      })
    })

    it('should load model with WebGPU execution provider if CUDA not available', async () => {
      // Setup model info
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.WEBGPU,
      } as any)

      // Setup listCachedModels with a CUDA model to indicate CUDA support
      vi.spyOn(manager, 'listCatalogModels').mockResolvedValue([])

      const mockResponse = {}
      vi.mocked(client.get).mockResolvedValue(mockResponse as any)

      await manager.loadModel('model_alias')

      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/load/model_id', {
        ttl: '600',
        ep: 'webgpu',
      })
    })

    it('should load model with CUDA execution provider if available', async () => {
      // Setup model info
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.WEBGPU,
      } as any)

      // Setup listCachedModels with a CUDA model to indicate CUDA support
      vi.spyOn(manager, 'listCatalogModels').mockResolvedValue([{ runtime: ExecutionProvider.CUDA } as any])

      const mockResponse = {}
      vi.mocked(client.get).mockResolvedValue(mockResponse as any)

      await manager.loadModel('model_alias')

      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/load/model_id', {
        ttl: '600',
        ep: 'cuda',
      })
    })

    it('should throw error if model has not been downloaded', async () => {
      // Setup model info
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.CPU,
      } as any)

      // Mock get to throw an error about model not found
      vi.mocked(client.get).mockRejectedValue(
        new Error('HTTP error! status: 404, response: No OpenAIService provider found for modelName'),
      )

      await expect(manager.loadModel('model_alias')).rejects.toThrow(
        'Model model_alias has not been downloaded yet. Please download it first',
      )
    })
  })

  describe('unloadModel', () => {
    it('should unload model if it is loaded', async () => {
      // Setup model info
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.CPU,
      } as any)

      // Setup listLoadedModels to include the model
      vi.spyOn(manager, 'listLoadedModels').mockResolvedValue([{ id: 'model_id' } as any])

      const mockResponse = {}
      vi.mocked(client.get).mockResolvedValue(mockResponse as any)

      await manager.unloadModel('model_alias')

      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/unload/model_id', {
        force: 'false',
      })
    })

    it('should do nothing if model is not loaded', async () => {
      // Setup model info
      vi.spyOn(manager, 'getModelInfo').mockResolvedValue({
        alias: 'model_alias',
        id: 'model_id',
        runtime: ExecutionProvider.CPU,
      } as any)

      // Setup listLoadedModels to not include the model
      vi.spyOn(manager, 'listLoadedModels').mockResolvedValue([])

      await manager.unloadModel('model_alias')

      // get should not be called to unload the model
      expect(client.get).not.toHaveBeenCalledWith(
        mockFetch,
        'http://localhost:5273/openai/unload/model_id',
        expect.anything(),
      )
    })
  })

  describe('listLoadedModels', () => {
    it('should fetch and return loaded models', async () => {
      // Create a similar test to listCachedModels
      const mockModelNames = ['model1', 'model2']
      const mockResponse = {
        json: vi.fn().mockResolvedValue(mockModelNames),
      }

      vi.mocked(client.get).mockResolvedValue(mockResponse as any)

      // Setup mock getModelInfo to return model info for each name
      vi.spyOn(manager, 'getModelInfo').mockImplementation(
        async (name) =>
          ({
            alias: `${name}_alias`,
            id: name,
            runtime: ExecutionProvider.CPU,
          }) as any,
      )

      const models = await manager.listLoadedModels()

      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/loadedmodels')
      expect(models).toHaveLength(2)
      expect(models[0]).toEqual({
        alias: 'model1_alias',
        id: 'model1',
        runtime: ExecutionProvider.CPU,
      })
    })
  })
})
