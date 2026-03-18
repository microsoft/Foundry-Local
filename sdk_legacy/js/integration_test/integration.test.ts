// Copyright (c) Microsoft.
// Licensed under the MIT License.

import { describe, it, expect, beforeAll } from 'vitest'
import { FoundryLocalManager } from '../src/index'
import type { FoundryModelInfo } from '../src/types'
import OpenAI from 'openai'

const MODELS = ['qwen2.5-0.5b', 'qwen2.5-0.5b-instruct-generic-cpu:3'] as const

let manager: FoundryLocalManager
let openai: OpenAI

beforeAll(async () => {
  manager = new FoundryLocalManager()
  await manager.startService()
  openai = new OpenAI({ baseURL: manager.endpoint, apiKey: manager.apiKey })
})

describe('FoundryLocalManager integration', () => {
  it('lists catalog models and prints unique execution providers', async () => {
    const models = await manager.listCatalogModels()
    expect(Array.isArray(models)).toBe(true)
    expect(models.length).toBeGreaterThan(0)

    for (const m of models) {
      expect(m.id).toBeTruthy()
      expect(m.alias).toBeTruthy()
    }

    console.log(`Found ${models.length} models in catalog: ${models.map((m) => m.id).join(', ')}`)

    const uniqueEPs = Array.from(new Set(models.map((m) => m.executionProvider))).sort()
    console.log(`Unique execution providers: ${uniqueEPs.join(', ')}`)
  })

  it('cache operations', async () => {
    const cacheDir = await manager.getCacheLocation()
    expect(typeof cacheDir).toBe('string')
    expect(cacheDir.length).toBeGreaterThan(0)

    const cached = await manager.listCachedModels()
    expect(Array.isArray(cached)).toBe(true)
  })

  it('service lifecycle', async () => {
    const running = await manager.isServiceRunning()
    expect(running).toBe(true)

    expect(manager.serviceUrl).toBeTruthy()
    expect(manager.endpoint.endsWith('/v1')).toBe(true)
    expect(typeof manager.apiKey).toBe('string')
  })

  // OpenAI chat streaming as a separate test case, run for both models
  it.each(MODELS)('OpenAI chat streaming for %s', async (modelId) => {
    const resolved: FoundryModelInfo | null = await manager.getModelInfo(modelId, undefined, true)
    expect(resolved?.id).toBeTruthy()

    // Download if not already present (no force)
    await manager.downloadModel(resolved!.id, undefined, undefined, false)

    // Ensure loaded
    await manager.loadModel(resolved!.id)

    // Stream a short chat completion
    const stream = await openai.chat.completions.create({
      model: resolved!.id,
      messages: [{ role: 'user', content: 'Why is the sky blue? Keep it brief.' }],
      stream: true,
    })

    process.stdout.write(`Response from model: ${modelId}: ${resolved!.id}\n`)
    let tokenCount = 0
    for await (const chunk of stream) {
      const delta = chunk.choices?.[0]?.delta?.content
      if (delta) {
        process.stdout.write(delta)
        tokenCount += delta.length
      }
    }
    process.stdout.write('\n')
    expect(tokenCount).toBeGreaterThan(0)
  })

  it.each(MODELS)('download → load → unload flow for %s', async (modelId) => {
    await manager.downloadModel(modelId, undefined, undefined, false)

    const info = await manager.loadModel(modelId)
    expect(info.id).toBeTruthy()

    const loaded1 = await manager.listLoadedModels()
    expect(loaded1.some((m) => m.id === info.id)).toBe(true)

    // unload without force → should remain loaded (TTL)
    await manager.unloadModel(modelId, undefined, false)
    const loaded2 = await manager.listLoadedModels()
    expect(loaded2.some((m) => m.id === info.id)).toBe(true)

    // unload with force → should disappear
    await manager.unloadModel(modelId, undefined, true)
    const loaded3 = await manager.listLoadedModels()
    expect(loaded3.some((m) => m.id === info.id)).toBe(false)
  })
})
