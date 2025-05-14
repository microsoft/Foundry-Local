// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import { exec, execSync } from 'child_process'

/**
 * Ensures that Foundry is available in the system's PATH.
 * Throws an error if Foundry is not installed or not found in the PATH.
 */
export function assertFoundryAvailable(): void {
  try {
    if (process.platform === 'win32') {
      execSync('where foundry')
    } else {
      execSync('which foundry')
    }
  } catch (error) {
    throw new Error('Foundry is not installed or not on PATH!')
  }
}

/**
 * Retrieves the service URL by executing the `foundry service status` command.
 * @returns {Promise<string | null>} The service URL if found, otherwise null.
 */
export async function getServiceUrl(): Promise<string | null> {
  try {
    const stdout = await new Promise<string>((resolve, reject) => {
      exec('foundry service status', (err: Error | null, stdout: string) => {
        if (err) {
          reject(err)
          return
        }
        resolve(stdout)
      })
    })
    const match = stdout.match(/http:\/\/(?:[a-zA-Z0-9.-]+|\d{1,3}(\.\d{1,3}){3}):\d+/)
    if (match) {
      return match[0]
    }
    return null
  } catch (error) {
    console.error('Error getting service status:', error)
    return null
  }
}

/**
 * Starts the Foundry service.
 * @returns {Promise<string | null>} The service URL if successfully started, otherwise null.
 */
export async function startService(): Promise<string | null> {
  let serviceUrl = await getServiceUrl()
  if (serviceUrl !== null) {
    return serviceUrl
  }

  // Start the service without waiting for stdout, it never completes
  exec('foundry service start', (err: Error | null) => {
    if (err) {
      console.error('Error starting service:', err.message)
      return null
    }
  })

  // Retry loop to check for service URL availability
  let retries = 10
  while (retries > 0) {
    serviceUrl = await getServiceUrl()
    if (serviceUrl !== null) {
      return serviceUrl
    }

    // Wait 100ms before trying again
    await new Promise((resolve) => setTimeout(resolve, 100))
    retries--
  }

  console.warn('Foundry service did not start within the expected time. May not be running.')
  return null
}
