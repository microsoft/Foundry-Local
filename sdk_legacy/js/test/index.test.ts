// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { describe, expect, it, vi, beforeEach } from 'vitest'
import { FoundryLocalManager } from '../src/index'
import * as service from '../src/service'
import { FoundryModelInfo, ExecutionProvider } from '../src/types'

// Mock service module
vi.mock('../src/service', () => ({
  assertFoundryAvailable: vi.fn(),
  getServiceUrl: vi.fn(),
  startService: vi.fn(),
}))

// Mock the base class
vi.mock('../src/base', () => {
  return {
    FoundryLocalManager: vi.fn().mockImplementation(() => ({
      downloadModel: vi.fn(),
      loadModel: vi.fn(),
    })),
  }
})

describe('FoundryLocalManager (index)', () => {
  let manager: FoundryLocalManager

  beforeEach(() => {
    vi.resetAllMocks()
    manager = new FoundryLocalManager()

    // Ensure these are spies in all tests that reference them
    manager.downloadModel = vi.fn()
    manager.loadModel = vi.fn()
  })

  describe('constructor', () => {
    it('should check if foundry is available', () => {
      expect(service.assertFoundryAvailable).toHaveBeenCalled()
    })
  })

  describe('serviceUrl', () => {
    it('should return the service URL if set', () => {
      // Accessing private field for testing
      manager['_serviceUrl'] = 'http://localhost:5273'
      expect(manager.serviceUrl).toBe('http://localhost:5273')
    })

    it('should throw error if service URL is not set', () => {
      manager['_serviceUrl'] = null
      expect(() => manager.serviceUrl).toThrow('Service URL is not set')
    })
  })

  describe('isServiceRunning', () => {
    it('should return true if service URL is found', async () => {
      vi.mocked(service.getServiceUrl).mockResolvedValue('http://localhost:5273')

      const result = await manager.isServiceRunning()

      expect(service.getServiceUrl).toHaveBeenCalled()
      expect(manager['_serviceUrl']).toBe('http://localhost:5273')
      expect(result).toBe(true)
    })

    it('should return false if service URL is not found', async () => {
      vi.mocked(service.getServiceUrl).mockResolvedValue(null)

      const result = await manager.isServiceRunning()

      expect(service.getServiceUrl).toHaveBeenCalled()
      expect(manager['_serviceUrl']).toBeNull()
      expect(result).toBe(false)
    })
  })

  describe('startService', () => {
    it('should set service URL if service starts successfully', async () => {
      vi.mocked(service.startService).mockResolvedValue('http://localhost:5273')

      await manager.startService()

      expect(service.startService).toHaveBeenCalled()
      expect(manager['_serviceUrl']).toBe('http://localhost:5273')
    })

    it('should throw if service fails to start', async () => {
      vi.mocked(service.startService).mockResolvedValue(null)

      await expect(manager.startService()).rejects.toThrow('Failed to start the service')
    })
  })

  describe('init', () => {
    it('should initialize with a model', async () => {
      vi.mocked(service.startService).mockResolvedValue('http://localhost:5273')

      const mockModelInfo: FoundryModelInfo = {
        alias: 'model_alias',
        id: 'model_name',
        version: '1',
        runtime: ExecutionProvider.CPU,
        uri: 'azureml://registries/azureml/models/model_name/versions/1',
        modelSize: 10403,
        promptTemplate: { prompt: '<|start|>{Content}<|end|>' },
        provider: 'AzureFoundry',
        publisher: 'Microsoft',
        license: 'MIT',
        task: 'chat-completion',
      }

      vi.mocked(manager.downloadModel).mockResolvedValue(undefined)
      vi.mocked(manager.loadModel).mockResolvedValue(mockModelInfo)

      const result = await manager.init('model_alias')

      expect(service.startService).toHaveBeenCalled()
      expect(manager.downloadModel).toHaveBeenCalledWith('model_alias', undefined)
      expect(manager.loadModel).toHaveBeenCalledWith('model_alias', undefined)
      expect(result).toEqual(mockModelInfo)
    })

    it('should initialize without a model', async () => {
      vi.mocked(service.startService).mockResolvedValue('http://localhost:5273')

      const result = await manager.init(null)

      expect(service.startService).toHaveBeenCalled()
      expect(manager.downloadModel).not.toHaveBeenCalled()
      expect(manager.loadModel).not.toHaveBeenCalled()
      expect(result).toBeNull()
    })
  })
})
