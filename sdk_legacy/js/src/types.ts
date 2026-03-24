// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

/**
 * Type alias for the fetch function.
 */
export type Fetch = typeof fetch

/**
 * Enum representing the types of devices.
 */
export enum DeviceType {
  CPU = 'CPU',
  GPU = 'GPU',
  NPU = 'NPU',
}

/**
 * Enum representing the execution providers.
 */
export enum ExecutionProvider {
  CPU = 'CPUExecutionProvider',
  WEBGPU = 'WebGpuExecutionProvider',
  CUDA = 'CUDAExecutionProvider',
}

/**
 * Interface representing the runtime configuration of a model.
 */
export interface ModelRuntime {
  /**
   * The type of device used for execution.
   */
  deviceType: DeviceType

  /**
   * The execution provider used for running the model.
   */
  executionProvider: string
}

/**
 * Interface representing the response model for a Foundry list operation.
 */
export interface FoundryListResponseModel {
  /**
   * The name of the model.
   */
  name: string

  /**
   * The display name of the model.
   */
  displayName: string

  /**
   * The type of the model.
   */
  modelType: string

  /**
   * The provider type of the model.
   */
  providerType: string

  /**
   * The URI of the model.
   */
  uri: string

  /**
   * The version of the model.
   */
  version: string

  /**
   * The prompt template associated with the model.
   */
  promptTemplate: Record<string, string>

  /**
   * The publisher of the model.
   */
  publisher: string

  /**
   * The task the model is designed for.
   */
  task: string

  /**
   * The runtime configuration of the model.
   */
  runtime: ModelRuntime

  /**
   * The file size of the model in megabytes.
   */
  fileSizeMb: number

  /**
   * The settings of the model.
   */
  modelSettings: Record<string, Record<string, string>[]>

  /**
   * The alias of the model.
   */
  alias: string

  /**
   * Indicates whether the model supports tool calling.
   */
  supportsToolCalling: boolean

  /**
   * The license of the model.
   */
  license: string

  /**
   * The description of the license.
   */
  licenseDescription: string

  /**
   * The URI of the parent model.
   */
  parentModelUri: string

  /**
   * The maximum number of output tokens.
   */
  maxOutputTokens: number

  /**
   * The minimum Foundry Local version required to use this model.
   */
  minFLVersion: string
}

/**
 * Interface representing information about a Foundry model.
 */
export interface FoundryModelInfo {
  /**
   * The alias of the model.
   */
  alias: string

  /**
   * The unique identifier of the model.
   */
  id: string

  /**
   * The version of the model.
   */
  version: string

  /**
   * The execution provider used for the model.
   */
  executionProvider: string

  /**
   * The device type used for the model.
   */
  deviceType: DeviceType

  /**
   * The URI of the model.
   */
  uri: string

  /**
   * The size of the model in megabytes.
   */
  modelSize: number

  /**
   * Indicates whether the model supports tool calling.
   */
  supportsToolCalling: boolean

  /**
   * The prompt template associated with the model.
   */
  promptTemplate: Record<string, string>

  /**
   * The provider of the model.
   */
  provider: string

  /**
   * The publisher of the model.
   */
  publisher: string

  /**
   * The license of the model.
   */
  license: string

  /**
   * The task the model is designed for.
   */
  task: string

  /**
   * EP Override
   */
  epOverride: string | null
}

/**
 * Interface representing the body of a download request.
 */
export interface DownloadBody {
  /**
   * The name of the model.
   */
  Name: string

  /**
   * The URI of the model.
   */
  Uri: string

  /**
   * The publisher of the model.
   */
  Publisher: string

  /**
   * The provider type of the model.
   */
  ProviderType: string

  /**
   * The prompt template associated with the model.
   */
  PromptTemplate: Record<string, string>
}
