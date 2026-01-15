// adapted from sdk_v2\cs\src\FoundryModelInfo.cs

export enum DeviceType {
    Invalid = 'Invalid',
    CPU = 'CPU',
    GPU = 'GPU',
    NPU = 'NPU'
}

export interface PromptTemplate {
    system?: string | null;
    user?: string | null;
    assistant: string;
    prompt: string;
}

export interface Runtime {
    deviceType: DeviceType;
    executionProvider: string;
}

export interface Parameter {
    name: string;
    value?: string | null;
}

export interface ModelSettings {
    parameters?: Parameter[] | null;
}

export interface ModelInfo {
    id: string;
    name: string;
    version: number;
    alias: string;
    displayName?: string | null;
    providerType: string;
    uri: string;
    modelType: string;
    promptTemplate?: PromptTemplate | null;
    publisher?: string | null;
    modelSettings?: ModelSettings | null;
    license?: string | null;
    licenseDescription?: string | null;
    cached: boolean;
    task?: string | null;
    runtime?: Runtime | null;
    fileSizeMb?: number | null;
    supportsToolCalling?: boolean | null;
    maxOutputTokens?: number | null;
    minFLVersion?: string | null;
    createdAtUnix: number;
}
