// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { FoundryLocalManager as FoundryLocalManagerBrowser } from './browser.js'
import * as service from './service.js'

import type { Config, FoundryModelInfo } from './interfaces.js'

/**
 * Class representing the Foundry Local Manager.
 */
export class FoundryLocalManager extends FoundryLocalManagerBrowser {
  /**
   * The service URL for the Foundry service.
   */
  protected _serviceUrl: string | null = null

  /**
   * Constructs a new FoundryLocalManager instance.
   * @param {Config} [Config] - Optional configuration object.
   */
  constructor(Config?: Config) {
    service.assertFoundryAvailable()
    super({
      serviceUrl: '',
      fetch: Config?.fetch,
    })
  }

  /**
   * Gets the service URL.
   * @throws {Error} If the service URL is not set.
   * @returns {string} The service URL.
   */
  get serviceUrl(): string {
    if (this._serviceUrl) {
      return this._serviceUrl
    }
    throw new Error('Service URL is not set. Please start the service first.')
  }

  /**
   * Initializes the Foundry Local Manager with a model.
   * @param {string | null} aliasOrModelId - The alias or ID of the model to initialize with.
   * @returns {Promise<FoundryModelInfo | null>} The model information if initialized, otherwise null.
   */
  async init(aliasOrModelId: string | null): Promise<FoundryModelInfo | null> {
    await this.startService()

    if (aliasOrModelId) {
      await this.downloadModel(aliasOrModelId)
      const modelInfo = await this.loadModel(aliasOrModelId)
      return modelInfo
    }
    return null
  }

  /**
   * Checks if the Foundry service is running.
   * @returns {Promise<boolean>} True if the service is running, otherwise false.
   */
  async isServiceRunning(): Promise<boolean> {
    this._serviceUrl = await service.getServiceUrl()
    return this._serviceUrl !== null
  }

  /**
   * Starts the Foundry service.
   * @throws {Error} If the service fails to start.
   * @returns {Promise<void>} Resolves when the service is successfully started.
   */
  async startService(): Promise<void> {
    this._serviceUrl = await service.startService()
    if (!this._serviceUrl) {
      throw new Error('Failed to start the service.')
    }
  }
}

export * from './interfaces.js'
