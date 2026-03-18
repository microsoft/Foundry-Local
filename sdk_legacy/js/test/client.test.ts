// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { describe, expect, it, vi, beforeEach } from 'vitest'
import * as client from '../src/client'
import type { Fetch } from '../src/types'
import { version } from '../src/version'

describe('Client', () => {
  const mockFetch = vi.fn() as unknown as Fetch
  const mockResponse = {
    ok: true,
    json: vi.fn(),
    text: vi.fn(),
    body: { getReader: vi.fn() },
    status: 200,
  }

  beforeEach(() => {
    vi.restoreAllMocks()
    mockFetch.mockReset()
    mockResponse.ok = true
    mockResponse.status = 200
    mockResponse.text.mockReset()
    mockResponse.json.mockReset()
    mockResponse.body.getReader.mockReset()
    mockFetch.mockResolvedValue(mockResponse as unknown as Response)
  })

  describe('get', () => {
    it('calls fetch with correct URL', async () => {
      await client.get(mockFetch, 'http://example.com')
      expect(mockFetch).toHaveBeenCalledWith('http://example.com', undefined)
    })

    it('appends query params', async () => {
      await client.get(mockFetch, 'http://example.com', { a: '1', b: '2' })
      expect(mockFetch).toHaveBeenCalledWith('http://example.com?a=1&b=2', undefined)
    })
  })

  describe('postWithProgress', () => {
    let mockReader: any
    let onProgress: ReturnType<typeof vi.fn>

    beforeEach(() => {
      onProgress = vi.fn()
      mockReader = { read: vi.fn() }
      mockResponse.body.getReader.mockReturnValue(mockReader)
    })

    it('sends POST with JSON', async () => {
      const body = { key: 'value' }

      mockReader.read
        .mockResolvedValueOnce({ done: false, value: new TextEncoder().encode('{"ok":true}') }) // valid JSON
        .mockResolvedValueOnce({ done: true, value: undefined })

      const result = await client.postWithProgress(mockFetch, 'http://example.com', body)

      expect(result).toEqual({ ok: true })
      expect(mockFetch).toHaveBeenCalledWith('http://example.com', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'User-Agent': `foundry-local-js-sdk/${version}`,
        },
        body: JSON.stringify(body),
      })
    })

    it('reports progress if onProgress is given', async () => {
      const mockOnProgress = vi.fn()
      const progress1 = new TextEncoder().encode('Progress: 50%')
      const progress2 = new TextEncoder().encode('Progress: 100%')
      const finalJson1 = new TextEncoder().encode('{"done"')
      const finalJson2 = new TextEncoder().encode(':true}')

      mockReader.read
        .mockResolvedValueOnce({ done: false, value: progress1 })
        .mockResolvedValueOnce({ done: false, value: progress2 })
        .mockResolvedValueOnce({ done: false, value: finalJson1 })
        .mockResolvedValueOnce({ done: false, value: finalJson2 })
        .mockResolvedValueOnce({ done: true, value: undefined })

      const result = await client.postWithProgress(mockFetch, 'http://example.com', {}, mockOnProgress)

      expect(mockOnProgress).toHaveBeenCalledWith(50)
      expect(mockOnProgress).toHaveBeenCalledWith(100)
      expect(result).toEqual({ done: true })
    })

    it('parses final JSON response', async () => {
      const jsonChunk = new TextEncoder().encode('{"message":"ok"}')
      mockReader.read
        .mockResolvedValueOnce({ done: false, value: jsonChunk })
        .mockResolvedValueOnce({ done: true, value: undefined })

      const result = await client.postWithProgress(mockFetch, 'http://example.com')
      expect(result).toEqual({ message: 'ok' })
    })

    it('throws on invalid JSON', async () => {
      mockReader.read
        .mockResolvedValueOnce({ done: false, value: new TextEncoder().encode('{invalid') })
        .mockResolvedValueOnce({ done: true, value: undefined })

      await expect(client.postWithProgress(mockFetch, 'http://example.com')).rejects.toThrow(
        /Error parsing JSON response/,
      )
    })

    it('throws if no JSON is received', async () => {
      mockReader.read.mockResolvedValueOnce({ done: true, value: undefined })

      await expect(client.postWithProgress(mockFetch, 'http://example.com')).rejects.toThrow('No JSON data received!')
    })
  })
})
