// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import type { Fetch } from './interfaces.js'

/**
 * Handles fetch requests with error handling.
 * @param {Fetch} fetch - The fetch function to use.
 * @param {string} url - The URL to fetch.
 * @param {RequestInit} [options] - Optional request options.
 * @returns {Promise<Response>} The fetch response.
 * @throws {Error} If the fetch request fails or returns a non-OK status.
 */
async function fetchWithErrorHandling(fetch: Fetch, url: string, options?: RequestInit): Promise<Response> {
  try {
    const response = await fetch(url, options)
    if (!response.ok) {
      let responseContent = ''
      try {
        responseContent = await response.text()
      } catch (error) {
        responseContent = 'Unable to read response content'
      }
      return Promise.reject(new Error(`HTTP error! status: ${response.status}, response: ${responseContent}`))
    }
    return response
  } catch (error) {
    const errorMessage = error instanceof Error ? error.message : 'Unknown error occurred'
    throw new Error(
      `Network error! Please check if the foundry service is running and the host URL is correct. Error: ${errorMessage}`,
    )
  }
}

/**
 * Sends a GET request to the specified host with optional query parameters.
 * @param {Fetch} fetch - The fetch function to use.
 * @param {string} host - The host URL.
 * @param {Record<string, string>} [queryParams] - Optional query parameters.
 * @returns {Promise<Response>} The fetch response.
 */
export const get = async (fetch: Fetch, host: string, queryParams?: Record<string, string>): Promise<Response> => {
  const endpoint = host + (queryParams ? '?' + new URLSearchParams(queryParams).toString() : '')
  return await fetchWithErrorHandling(fetch, endpoint)
}

/**
 * Parses a percentage value from a string.
 * @param {string} line - The string containing the percentage value.
 * @returns {number | null} The parsed percentage value, or null if not found.
 */
function parsePercentage(line: string): number | null {
  const match = line.match(/(\d+(?:\.\d+)?)%/)
  return match ? Math.min(parseInt(match[1]), 100) : null
}

/**
 * Sends a POST request with progress updates.
 * @param {Fetch} fetch - The fetch function to use.
 * @param {string} host - The host URL.
 * @param {Record<string, unknown>} [body] - The request body.
 * @param {(progress: number) => void} [onProgress] - Optional progress callback.
 * @returns {Promise<Record<string, unknown>>} The parsed response body.
 */
export const postWithProgress = async (
  fetch: Fetch,
  host: string,
  body?: Record<string, unknown>,
  onProgress?: (progress: number) => void,
): Promise<Record<string, unknown>> => {
  // Sending a POST request and getting a streamable response
  const response = await fetchWithErrorHandling(fetch, host, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: body ? JSON.stringify(body) : undefined,
  })

  // Set up progress tracking
  const reader = response.body?.getReader()
  let finalJson = ''
  let prevPercent = 0

  // Read and process the response stream
  while (true) {
    const { done, value } = (await reader?.read()) ?? {
      done: true,
      value: new Uint8Array(),
    }

    if (done) {
      break
    }

    const line = new TextDecoder('utf-8').decode(value)

    // Accumulate the final JSON when we start receiving JSON data
    if (finalJson || line.startsWith('{')) {
      finalJson += line
      continue
    }

    // If onProgress is not provided, skip progress tracking
    if (!onProgress) {
      continue
    }

    // Parse progress percentage from the line
    const percent = parsePercentage(line)
    if (percent !== null && percent > prevPercent) {
      prevPercent = percent
      onProgress(percent) // Update the progress
    }
  }

  // Parse and return the final JSON response
  if (finalJson) {
    try {
      return JSON.parse(finalJson)
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : 'Unknown error occurred'
      throw new Error(`Error parsing JSON response: ${errorMessage}`)
    }
  } else {
    throw new Error('No JSON data received!')
  }
}
