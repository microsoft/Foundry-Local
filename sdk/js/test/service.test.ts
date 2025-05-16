// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { describe, expect, it, vi, beforeEach } from 'vitest'
import * as service from '../src/service'
import * as child_process from 'child_process'

vi.mock('child_process', () => ({
  exec: vi.fn(),
  execSync: vi.fn(),
}))

describe('Service', () => {
  beforeEach(() => {
    vi.restoreAllMocks()
    vi.resetAllMocks()
  })

  describe('assertFoundryAvailable', () => {
    it('should not throw error if foundry is available on Windows', () => {
      vi.spyOn(process, 'platform', 'get').mockReturnValue('win32')
      vi.mocked(child_process.execSync).mockReturnValue(Buffer.from('C:\\foundry.exe'))

      expect(() => service.assertFoundryAvailable()).not.toThrow()
      expect(child_process.execSync).toHaveBeenCalledWith('where foundry')
    })

    it('should not throw error if foundry is available on non-Windows', () => {
      vi.spyOn(process, 'platform', 'get').mockReturnValue('darwin')
      vi.mocked(child_process.execSync).mockReturnValue(Buffer.from('/usr/local/bin/foundry'))

      expect(() => service.assertFoundryAvailable()).not.toThrow()
      expect(child_process.execSync).toHaveBeenCalledWith('which foundry')
    })

    it('should throw error if foundry is not available', () => {
      vi.mocked(child_process.execSync).mockImplementation(() => {
        throw new Error('Command failed')
      })

      expect(() => service.assertFoundryAvailable()).toThrow('Foundry is not installed or not on PATH')
    })
  })

  describe('getServiceUrl', () => {
    it('should extract service URL from command output', async () => {
      vi.mocked(child_process.exec).mockImplementation((cmd, callback) => {
        callback(null, 'Model management service is running on http://localhost:5273/openai/status')
      })

      const url = await service.getServiceUrl()
      expect(url).toBe('http://localhost:5273')
    })

    it('should handle IP address in service URL', async () => {
      vi.mocked(child_process.exec).mockImplementation((cmd, callback) => {
        callback(null, 'Model management service is running on http://127.0.0.1:5273/openai/status')
      })

      const url = await service.getServiceUrl()
      expect(url).toBe('http://127.0.0.1:5273')
    })

    it('should return null if no URL is found in output', async () => {
      vi.mocked(child_process.exec).mockImplementation((cmd, callback) => {
        callback(null, 'Model management service is not running!')
      })

      const url = await service.getServiceUrl()
      expect(url).toBeNull()
    })

    it('should return null if command fails', async () => {
      vi.spyOn(console, 'error').mockImplementation(() => {})
      vi.mocked(child_process.exec).mockImplementation((cmd, callback) => {
        callback(new Error('Command failed'), '')
      })

      const url = await service.getServiceUrl()
      expect(url).toBeNull()
      expect(console.error).toHaveBeenCalled()
    })
  })

  describe('startService', () => {
    it('should return existing service URL if service is already running', async () => {
      vi.mocked(child_process.exec).mockImplementation((cmd, callback) => {
        callback(null, 'Model management service is running on http://localhost:5273/openai/status')
      })

      const url = await service.startService()

      expect(url).toBe('http://localhost:5273')
      expect(child_process.exec).not.toHaveBeenCalledWith('foundry service start', expect.any(Function))
    })

    it('should start service and return URL after retry', async () => {
      vi.useFakeTimers()

      let callCount = 0
      vi.mocked(child_process.exec).mockImplementation((cmd, callback) => {
        if (cmd === 'foundry service status') {
          if (callCount++ === 0) {
            callback(null, 'Model management service is not running!')
          } else {
            callback(null, 'Model management service is running on http://localhost:5273/openai/status')
          }
        } else if (cmd === 'foundry service start') {
          callback(null)
        }
      })

      const promise = service.startService()
      await vi.advanceTimersByTimeAsync(100)
      const url = await promise

      expect(url).toBe('http://localhost:5273')
      expect(child_process.exec).toHaveBeenCalledWith('foundry service start', expect.any(Function))

      vi.useRealTimers()
    })

    it('should return null if service fails to start after retries', async () => {
      vi.useFakeTimers()
      vi.spyOn(console, 'warn').mockImplementation(() => {})

      // Always return status output with no URL
      vi.mocked(child_process.exec).mockImplementation((cmd, callback) => {
        if (cmd === 'foundry service status') {
          callback(null, 'Model management service is not running!')
        } else if (cmd === 'foundry service start') {
          callback(null)
        }
      })

      const promise = service.startService()
      await vi.advanceTimersByTimeAsync(1000) // 10 * 100ms retry delay
      const url = await promise

      expect(url).toBeNull()
      expect(child_process.exec).toHaveBeenCalledWith('foundry service start', expect.any(Function))
      expect(console.warn).toHaveBeenCalledWith(
        'Foundry service did not start within the expected time. May not be running.',
      )

      vi.useRealTimers()
    })
  })
})
