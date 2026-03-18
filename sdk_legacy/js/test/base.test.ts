// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { describe, expect, it, vi, beforeEach, afterEach } from 'vitest'
import { FoundryLocalManager } from '../src/base'
import * as client from '../src/client'
import { DeviceType, type FoundryModelInfo } from '../src/types'

vi.mock('../src/client', () => ({
  get: vi.fn(),
  postWithProgress: vi.fn(),
}))

const MOCK_INFO = {
  providerType: 'AzureFoundry',
  version: '1',
  modelType: 'ONNX',
  promptTemplate: { prompt: '<|im_start|>user<|im_sep|>{Content}<|im_end|><|im_start|>assistant<|im_sep|>' },
  publisher: 'Microsoft',
  task: 'chat-completion',
  fileSizeMb: 10403,
  modelSettings: { parameters: [] },
  supportsToolCalling: false,
  license: 'MIT',
  licenseDescription: 'License…',
  maxOutputTokens: 1024,
}

const MOCK_CATALOG_DATA = [
  // generic-gpu, generic-cpu (alias: model-1)
  {
    name: 'model-1-generic-gpu:1',
    displayName: 'model-1-generic-gpu',
    uri: 'azureml://registries/azureml/models/model-1-generic-gpu/versions/1',
    runtime: { deviceType: 'GPU', executionProvider: 'WebGpuExecutionProvider' },
    alias: 'model-1',
    parentModelUri: 'azureml://registries/azureml/models/model-1/versions/1',
    ...MOCK_INFO,
  },
  // newer CPU version also present
  {
    name: 'model-1-generic-cpu:2',
    displayName: 'model-1-generic-cpu',
    uri: 'azureml://registries/azureml/models/model-1-generic-cpu/versions/2',
    runtime: { deviceType: 'CPU', executionProvider: 'CPUExecutionProvider' },
    alias: 'model-1',
    parentModelUri: 'azureml://registries/azureml/models/model-1/versions/2',
    ...MOCK_INFO,
  },
  {
    name: 'model-1-generic-cpu:1',
    displayName: 'model-1-generic-cpu',
    uri: 'azureml://registries/azureml/models/model-1-generic-cpu/versions/1',
    runtime: { deviceType: 'CPU', executionProvider: 'CPUExecutionProvider' },
    alias: 'model-1',
    parentModelUri: 'azureml://registries/azureml/models/model-1/versions/1',
    ...MOCK_INFO,
  },

  // npu + generic-cpu (alias: model-2)
  {
    name: 'model-2-npu:2',
    displayName: 'model-2-npu',
    uri: 'azureml://registries/azureml/models/model-2-npu/versions/2',
    runtime: { deviceType: 'NPU', executionProvider: 'QNNExecutionProvider' },
    alias: 'model-2',
    parentModelUri: 'azureml://registries/azureml/models/model-2/versions/2',
    ...MOCK_INFO,
  },
  {
    name: 'model-2-npu:1',
    displayName: 'model-2-npu',
    uri: 'azureml://registries/azureml/models/model-2-npu/versions/1',
    runtime: { deviceType: 'NPU', executionProvider: 'QNNExecutionProvider' },
    alias: 'model-2',
    parentModelUri: 'azureml://registries/azureml/models/model-2/versions/1',
    ...MOCK_INFO,
  },
  {
    name: 'model-2-generic-cpu:1',
    displayName: 'model-2-generic-cpu',
    uri: 'azureml://registries/azureml/models/model-2-generic-cpu/versions/1',
    runtime: { deviceType: 'CPU', executionProvider: 'CPUExecutionProvider' },
    alias: 'model-2',
    parentModelUri: 'azureml://registries/azureml/models/model-2/versions/1',
    ...MOCK_INFO,
  },

  // cuda-gpu + generic-gpu + generic-cpu (alias: model-3)
  {
    name: 'model-3-cuda-gpu:1',
    displayName: 'model-3-cuda-gpu',
    uri: 'azureml://registries/azureml/models/model-3-cuda-gpu/versions/1',
    runtime: { deviceType: 'GPU', executionProvider: 'CUDAExecutionProvider' },
    alias: 'model-3',
    parentModelUri: 'azureml://registries/azureml/models/model-3/versions/1',
    ...MOCK_INFO,
  },
  {
    name: 'model-3-generic-gpu:1',
    displayName: 'model-3-generic-gpu',
    uri: 'azureml://registries/azureml/models/model-3-generic-gpu/versions/1',
    runtime: { deviceType: 'GPU', executionProvider: 'WebGpuExecutionProvider' },
    alias: 'model-3',
    parentModelUri: 'azureml://registries/azureml/models/model-3/versions/1',
    ...MOCK_INFO,
  },
  {
    name: 'model-3-generic-cpu:1',
    displayName: 'model-3-generic-cpu',
    uri: 'azureml://registries/azureml/models/model-3-generic-cpu/versions/1',
    runtime: { deviceType: 'CPU', executionProvider: 'CPUExecutionProvider' },
    alias: 'model-3',
    parentModelUri: 'azureml://registries/azureml/models/model-3/versions/1',
    ...MOCK_INFO,
  },

  // generic-gpu only (alias: model-4)
  {
    name: 'model-4-generic-gpu:1',
    displayName: 'model-4-generic-gpu',
    uri: 'azureml://registries/azureml/models/model-4-generic-gpu/versions/1',
    runtime: { deviceType: 'GPU', executionProvider: 'WebGpuExecutionProvider' },
    alias: 'model-4',
    parentModelUri: 'azureml://registries/azureml/models/model-4/versions/1',
    ...MOCK_INFO,
    promptTemplate: null,
    newFeature: 'newValue',
    minFLVersion: '1.0.0',
  },
]

// Mock response for /openai/status
const MOCK_STATUS_RESPONSE = { modelDirPath: '/test/path/to/models' }
// Mock response for /openai/models
const MOCK_LOCAL_MODELS = ['model-2-npu:1', 'model-4-generic-gpu:1']
// Mock response for /openai/loadedmodels
const MOCK_LOADED_MODELS = ['model-2-npu:1']

function makeJsonResponse(payload: any) {
  return { json: vi.fn().mockResolvedValue(payload) } as any
}

// Helper to set up /foundry/list with or without CUDA entries
function mockFoundryList(useCuda: boolean) {
  const data = useCuda ? MOCK_CATALOG_DATA : MOCK_CATALOG_DATA.filter((m) => !m.name.includes('cuda'))
  vi.mocked(client.get).mockImplementation(async (fetchFn, url) => {
    if (url.endsWith('/foundry/list')) return makeJsonResponse(data)
    if (url.endsWith('/openai/status')) return makeJsonResponse(MOCK_STATUS_RESPONSE)
    if (url.endsWith('/openai/models')) return makeJsonResponse(MOCK_LOCAL_MODELS)
    if (url.endsWith('/openai/loadedmodels')) return makeJsonResponse(MOCK_LOADED_MODELS)
    // load/unload/download calls don’t need to return JSON in these tests
    return {} as any
  })
}

describe('FoundryLocalManager (base.ts)', () => {
  let manager: FoundryLocalManager
  const mockFetch = vi.fn()

  beforeEach(() => {
    vi.resetAllMocks()
    manager = new FoundryLocalManager({ serviceUrl: 'http://localhost:5273', fetch: mockFetch as any })
  })

  afterEach(() => {
    vi.restoreAllMocks()
  })

  describe('constructor & props', () => {
    it('sets serviceUrl and fetch; endpoint/apiKey work', () => {
      expect(manager['__proto__']).toBe(FoundryLocalManager.prototype)
      expect(manager['fetch']).toBe(mockFetch)
      expect(manager.serviceUrl).toBe('http://localhost:5273')
      expect(manager.endpoint).toBe('http://localhost:5273/v1')
      expect(manager.apiKey).toBe('OPENAI_API_KEY')
    })

    it('throws if serviceUrl missing', () => {
      // @ts-expect-error force bad state for test
      manager['_serviceUrl'] = null
      expect(() => manager.serviceUrl).toThrow('Service URL is invalid')
    })
  })

  describe('listCatalogModels', () => {
    it('maps Foundry list → FoundryModelInfo and caches; sets epOverride for generic-gpu when CUDA present', async () => {
      mockFoundryList(true) // CUDA present
      const models = await manager.listCatalogModels()

      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/foundry/list')
      expect(Array.isArray(models)).toBe(true)
      const ids = models.map((m) => m.id)
      expect(ids).toEqual([
        'model-1-generic-gpu:1',
        'model-1-generic-cpu:2',
        'model-1-generic-cpu:1',
        'model-2-npu:2',
        'model-2-npu:1',
        'model-2-generic-cpu:1',
        'model-3-cuda-gpu:1',
        'model-3-generic-gpu:1',
        'model-3-generic-cpu:1',
        'model-4-generic-gpu:1',
      ])

      // spot-check shape
      const m0 = models[0]
      expect(m0).toMatchObject<Partial<FoundryModelInfo>>({
        alias: 'model-1',
        id: 'model-1-generic-gpu:1',
        version: '1',
        executionProvider: 'WebGpuExecutionProvider',
        deviceType: 'GPU',
        provider: 'AzureFoundry',
        publisher: 'Microsoft',
        task: 'chat-completion',
      })

      // CUDA present → generic-gpu entries should have epOverride='cuda'
      const genericGpu = models.filter((m) => m.id.includes('generic-gpu'))
      expect(genericGpu.every((m) => m.epOverride === 'cuda')).toBe(true)

      // second call uses cache (no extra /foundry/list)
      await manager.listCatalogModels()
      expect((client.get as any).mock.calls.filter(([, url]: any[]) => url.endsWith('/foundry/list')).length).toBe(1)
    })

    it('no CUDA present → epOverride stays null', async () => {
      mockFoundryList(false)
      const models = await manager.listCatalogModels()
      const genericGpu = models.filter((m) => m.id.includes('generic-gpu'))
      expect(genericGpu.every((m) => m.epOverride == null)).toBe(true)
    })

    it('refreshCatalog clears cache', async () => {
      mockFoundryList(true)
      await manager.listCatalogModels()
      await manager.refreshCatalog()
      await manager.listCatalogModels()
      // called twice total
      expect((client.get as any).mock.calls.filter(([, url]: any[]) => url.endsWith('/foundry/list')).length).toBe(2)
    })
  })

  describe('getModelInfo (id/alias/device & Windows fallback)', () => {
    async function setup(useCuda: boolean) {
      mockFoundryList(useCuda)
      await manager.listCatalogModels()
    }

    it('returns exact ID if includes version; picks highest version for ID prefix', async () => {
      await setup(true)
      expect((await manager.getModelInfo('model-1-generic-cpu:1'))?.id).toBe('model-1-generic-cpu:1')
      expect((await manager.getModelInfo('model-1-generic-cpu'))?.id).toBe('model-1-generic-cpu:2')
    })

    it('alias selection: prefers NPU > CUDA > WebGPU > CPU by availability; device filter works', async () => {
      await setup(true)

      // alias default
      expect((await manager.getModelInfo('model-2'))?.id).toBe('model-2-npu:2')
      expect((await manager.getModelInfo('model-3'))?.id).toBe('model-3-cuda-gpu:1')

      // device filter
      expect((await manager.getModelInfo('model-1', DeviceType.GPU))?.id).toBe('model-1-generic-gpu:1')
      expect((await manager.getModelInfo('model-1', DeviceType.CPU))?.id).toBe('model-1-generic-cpu:2')
      expect(await manager.getModelInfo('model-1', DeviceType.NPU)).toBeNull()
      expect((await manager.getModelInfo('model-2', DeviceType.NPU))?.id).toBe('model-2-npu:2')
      expect((await manager.getModelInfo('model-2', DeviceType.CPU))?.id).toBe('model-2-generic-cpu:1')
      expect(await manager.getModelInfo('model-2', DeviceType.GPU)).toBeNull()
      expect((await manager.getModelInfo('model-3', DeviceType.GPU))?.id).toBe('model-3-cuda-gpu:1')
      expect((await manager.getModelInfo('model-3', DeviceType.CPU))?.id).toBe('model-3-generic-cpu:1')
      expect(await manager.getModelInfo('model-3', DeviceType.NPU)).toBeNull()
    })

    it('unknown returns null; throwOnNotFound flips to error', async () => {
      await setup(true)
      expect(await manager.getModelInfo('unknown-model')).toBeNull()
      await expect(manager.getModelInfo('unknown-model', undefined, true)).rejects.toThrow(
        'Model unknown-model not found in the catalog.',
      )
    })

    it('Windows fallback: if alias picks generic-gpu WITH NO override, prefer CPU variant when present', async () => {
      // Force "no CUDA support" to keep epOverride null for generic-gpu
      await setup(false)

      // Shim platform detection: process.platform = win32
      const orig = Object.getOwnPropertyDescriptor(process, 'platform')
      Object.defineProperty(process, 'platform', { get: () => 'win32' })

      try {
        // alias model-1 has generic-gpu and generic-cpu; with no epOverride, Windows should pick CPU
        expect((await manager.getModelInfo('model-1'))?.id).toBe('model-1-generic-cpu:2')

        // For model-3, since we removed CUDA entries, only generic-gpu/cpu remain → should pick CPU as well
        expect((await manager.getModelInfo('model-3'))?.id).toBe('model-3-generic-cpu:1')
      } finally {
        if (orig) Object.defineProperty(process, 'platform', orig)
      }
    })
  })

  describe('getCacheLocation', () => {
    it('returns modelDirPath from /openai/status', async () => {
      mockFoundryList(true)
      const location = await manager.getCacheLocation()
      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/status')
      expect(location).toBe('/test/path/to/models')
    })
  })

  describe('listCachedModels & listLoadedModels', () => {
    beforeEach(async () => {
      mockFoundryList(true)
      await manager.listCatalogModels()
      // spy getModelInfo to return catalog-mapped objects quickly
      vi.spyOn(manager, 'getModelInfo').mockImplementation(async (idOrAlias: string) => {
        const id = idOrAlias
        const m = (await manager.listCatalogModels()).find((mm) => mm.id === id || mm.alias === idOrAlias)
        return (m ?? null) as any
      })
    })

    it('listCachedModels returns mapped infos', async () => {
      const models = await manager.listCachedModels()
      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/models')
      const ids = models.map((m) => m.id)
      expect(ids).toEqual(['model-2-npu:1', 'model-4-generic-gpu:1'])
    })

    it('listLoadedModels returns mapped infos', async () => {
      const models = await manager.listLoadedModels()
      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/loadedmodels')
      const ids = models.map((m) => m.id)
      expect(ids).toEqual(['model-2-npu:1'])
    })
  })

  describe('downloadModel', () => {
    beforeEach(async () => {
      mockFoundryList(true)
      await manager.listCatalogModels()
    })

    it('skips when already cached and force=false (calls onProgress(100))', async () => {
      // Mock cache contains the target ID
      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([{ id: 'model-3-cuda-gpu:1' } as any])
      const onProgress = vi.fn()
      const info = await manager.downloadModel('model-3', undefined, undefined, false, onProgress)
      expect(info.id).toBe('model-3-cuda-gpu:1')
      expect(onProgress).toHaveBeenCalledWith(100)
      expect(client.postWithProgress).not.toHaveBeenCalled()
    })

    it('posts to /openai/download when not cached; providerType AzureFoundry → AzureFoundryLocal', async () => {
      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([])
      vi.mocked(client.postWithProgress).mockResolvedValue({ success: true } as any)

      const info = await manager.downloadModel('model-1-generic-cpu:2')
      expect(info.id).toBe('model-1-generic-cpu:2')

      expect(client.postWithProgress).toHaveBeenCalledWith(
        mockFetch,
        'http://localhost:5273/openai/download',
        {
          model: {
            Name: 'model-1-generic-cpu:2',
            Uri: 'azureml://registries/azureml/models/model-1-generic-cpu/versions/2',
            Publisher: 'Microsoft',
            ProviderType: 'AzureFoundryLocal',
            PromptTemplate: MOCK_INFO.promptTemplate,
          },
          IgnorePipeReport: true,
        },
        undefined,
      )
    })

    it('throws on download error', async () => {
      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([])
      vi.mocked(client.postWithProgress).mockResolvedValue({ success: false, error: 'Download failed' } as any)
      await expect(manager.downloadModel('model-1')).rejects.toThrow(
        "Failed to download model with alias 'model-1' and ID 'model-1-generic-gpu:1': Download failed",
      )
    })
  })

  describe('isModelUpgradeable', () => {
    beforeEach(async () => {
      mockFoundryList(true)
      await manager.listCatalogModels()
    })

    it('true when latest ID not present in cache', async () => {
      // latest for model-1 alias is CPU:2 (highest version for that ID prefix)
      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([])
      expect(await manager.isModelUpgradeable('model-1')).toBe(true)
    })

    it('false when cached contains the exact latest ID', async () => {
      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([{ id: 'model-4-generic-gpu:1' } as any])
      expect(await manager.isModelUpgradeable('model-4')).toBe(false)
    })

    it('true when cached has an older version', async () => {
      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([{ id: 'model-2-npu:1' } as any])
      expect(await manager.isModelUpgradeable('model-2')).toBe(true)
    })

    it('false when version cannot be parsed', async () => {
      // Spy getLatestModelInfo to return an ID with no version suffix
      // @ts-expect-error accessing private for test via cast
      vi.spyOn(manager as any, 'getLatestModelInfo').mockResolvedValue({
        id: 'model-invalid-version',
        alias: 'model-invalid',
      } as FoundryModelInfo)
      vi.spyOn(manager, 'listCachedModels').mockResolvedValue([])
      expect(await manager.isModelUpgradeable('model-invalid-version')).toBe(false)
    })
  })

  describe('upgradeModel', () => {
    beforeEach(async () => {
      mockFoundryList(true)
      await manager.listCatalogModels()
    })

    it('downloads latest (not in cache case)', async () => {
      const latest = (await manager.getModelInfo('model-3')) as FoundryModelInfo // cuda
      const dl = vi.spyOn(manager, 'downloadModel').mockResolvedValue(latest)
      // @ts-expect-error test private by behavior
      const res = await manager.upgradeModel('model-3')
      expect(dl).toHaveBeenCalledWith(latest.id, undefined, undefined, false, undefined)
      expect(res.id).toBe('model-3-cuda-gpu:1')
    })

    it('uses latest when older version in cache', async () => {
      const latest = (await manager.getModelInfo('model-2')) as FoundryModelInfo // npu:2
      const dl = vi.spyOn(manager, 'downloadModel').mockResolvedValue(latest)
      const res = await manager.upgradeModel('model-2-npu:1')
      expect(dl).toHaveBeenCalledWith('model-2-npu:2', undefined, undefined, false, undefined)
      expect(res.id).toBe('model-2-npu:2')
    })

    it('no re-download if already latest (call still targets latest id)', async () => {
      const latest = (await manager.getModelInfo('model-4')) as FoundryModelInfo // single version
      const dl = vi.spyOn(manager, 'downloadModel').mockResolvedValue(latest)
      const res = await manager.upgradeModel('model-4')
      expect(dl).toHaveBeenCalledWith('model-4-generic-gpu:1', undefined, undefined, false, undefined)
      expect(res.id).toBe('model-4-generic-gpu:1')
    })
  })

  describe('loadModel', () => {
    beforeEach(async () => {
      vi.resetAllMocks()
    })

    it('loads with TTL only; no ep param when no override', async () => {
      // No CUDA present → epOverride stays null for generic-gpu
      mockFoundryList(false)
      const info = await new FoundryLocalManager({
        serviceUrl: 'http://localhost:5273',
        fetch: mockFetch as any,
      }).loadModel('model-2') // picks model-2-npu:2
      expect(info.id).toBe('model-2-npu:2')
      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/load/model-2-npu:2', {
        ttl: '600',
      })
    })

    it('adds ep=cuda for generic-gpu when CUDA support present (epOverride set by listCatalogModels)', async () => {
      mockFoundryList(true)
      // Force a fresh manager so it recomputes catalog and overrides
      const mgr = new FoundryLocalManager({ serviceUrl: 'http://localhost:5273', fetch: mockFetch as any })
      // load alias 'model-4' → generic-gpu with epOverride='cuda'
      const info = await mgr.loadModel('model-4')
      expect(info.id).toBe('model-4-generic-gpu:1')
      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/load/model-4-generic-gpu:1', {
        ttl: '600',
        ep: 'cuda',
      })
    })

    it('throws a friendly message when not downloaded', async () => {
      mockFoundryList(true)

      // Capture whatever behavior is currently configured for client.get
      const original = vi.mocked(client.get).getMockImplementation()

      // Intercept only the load URL; delegate all else to original
      vi.mocked(client.get).mockImplementation((fetchFn: any, url: string, query?: any) => {
        if (url.includes('/openai/load/')) {
          return Promise.reject(new Error('No OpenAIService provider found for modelName'))
        }
        return original ? original(fetchFn, url, query) : Promise.resolve({ json: async () => ({}) } as any)
      })

      await expect(manager.loadModel('model-3')).rejects.toThrow(
        'Model model-3 has not been downloaded yet. Please download it first.',
      )
    })
  })

  describe('unloadModel', () => {
    beforeEach(async () => {
      mockFoundryList(true)
      await manager.listCatalogModels()
    })

    it('unloads when loaded', async () => {
      // model-2-npu:1 is in MOCK_LOADED_MODELS
      await manager.unloadModel('model-2-npu:1')
      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/unload/model-2-npu:1', {
        force: 'false',
      })
    })

    it('no-op when not loaded', async () => {
      vi.mocked(client.get).mockClear()
      await manager.unloadModel('model-4') // not in loaded list
      const calls = (client.get as any).mock.calls.map(([, url]: any[]) => url)
      expect(calls.some((u: string) => u.endsWith('/openai/unload/model-4-generic-gpu:1'))).toBe(false)
    })

    it('force unload', async () => {
      await manager.unloadModel('model-2-npu:1', undefined, true as any)
      expect(client.get).toHaveBeenCalledWith(mockFetch, 'http://localhost:5273/openai/unload/model-2-npu:1', {
        force: 'true',
      })
    })
  })
})
