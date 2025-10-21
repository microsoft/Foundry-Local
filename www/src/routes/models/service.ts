// API service for Azure AI Foundry models
// Uses Azure Function as CORS proxy for static deployment
import type { FoundryModel, GroupedFoundryModel } from './types';

// Azure Function endpoint for CORS proxy
const FOUNDRY_API_ENDPOINT =
	'https://onnxruntime-foundry-proxy-hpape7gzf2haesef.eastus-01.azurewebsites.net/api/foundryproxy';

export interface ApiFilters {
	device?: string;
	family?: string;
	acceleration?: string;
	searchTerm?: string;
}

export interface ApiSortOptions {
	sortBy: string;
	sortOrder: 'asc' | 'desc';
}

export class FoundryModelService {
	// Cache for all models - fetched once
	private allModelsCache: FoundryModel[] | null = null;
	private fetchPromise: Promise<FoundryModel[]> | null = null;

	// Blocked model IDs that should not be displayed
	private readonly BLOCKED_MODEL_IDS = new Set([
		'deepseek-r1-distill-qwen-1.5b-generic-cpu',
		'deepseek-r1-distill-qwen-7b-generic-cpu',
		'deepseek-r1-distill-llama-8b-generic-cpu',
		'deepseek-r1-distill-qwen-14b-generic-cpu',
		'Phi-4-mini-instruct-generic-cpu',
		'deepseek-r1-distill-llama-8b-cuda-gpu',
		'deepseek-r1-distill-llama-8b-generic-gpu'
	]);

	// Detect acceleration from model name
	private detectAcceleration(modelName: string): string | undefined {
		const nameLower = modelName.toLowerCase();
		if (nameLower.includes('-qnn-') || nameLower.includes('-qnn')) {
			return 'qnn';
		}
		if (nameLower.includes('-vitis-') || nameLower.includes('-vitis')) {
			return 'vitis';
		}
		if (nameLower.includes('-openvino-') || nameLower.includes('-openvino')) {
			return 'openvino';
		}
		if (
			nameLower.includes('-trt-rtx-') ||
			nameLower.includes('-tensorrt-') ||
			nameLower.includes('-trt-rtx') ||
			nameLower.includes('-tensorrt')
		) {
			return 'trt-rtx';
		}
		if (nameLower.includes('-cuda-') || nameLower.includes('-cuda')) {
			return 'cuda';
		}
		if (nameLower.includes('-generic-gpu') || nameLower.includes('webgpu')) {
			return 'webgpu';
		}
		return undefined;
	}

	// Get acceleration display name
	getAccelerationDisplayName(acceleration: string): string {
		const accelerationNames: Record<string, string> = {
			qnn: 'Qualcomm QNN',
			vitis: 'AMD Vitis AI',
			openvino: 'Intel OpenVINO',
			'trt-rtx': 'NVIDIA TensorRT RTX',
			cuda: 'NVIDIA CUDA',
			webgpu: 'WebGPU'
		};
		return accelerationNames[acceleration] || acceleration;
	}

	// Fetch all models from API once and cache them
	async fetchAllModels(): Promise<FoundryModel[]> {
		// Return cached models if available
		if (this.allModelsCache) {
			return this.allModelsCache;
		}

		// If already fetching, return the existing promise
		if (this.fetchPromise) {
			return this.fetchPromise;
		}

		// Create fetch promise
		this.fetchPromise = this.fetchModelsFromAPI();

		try {
			this.allModelsCache = await this.fetchPromise;
			return this.allModelsCache;
		} finally {
			this.fetchPromise = null;
		}
	}

	// Clear cache if needed (for refresh)
	clearCache(): void {
		this.allModelsCache = null;
		this.fetchPromise = null;
	}

	async fetchModels(
		filters: ApiFilters = {},
		sortOptions: ApiSortOptions = { sortBy: 'name', sortOrder: 'asc' }
	): Promise<FoundryModel[]> {
		// Get all models from cache or API
		const allModels = await this.fetchAllModels();

		// Apply filters and sorting client-side
		const filteredModels = this.applyClientSideProcessing(allModels, filters, sortOptions);

		return filteredModels;
	}

	private async fetchModelsFromAPI(): Promise<FoundryModel[]> {
		const devices = ['npu', 'gpu', 'cpu'];
		const allModels: FoundryModel[] = [];
		const seenModelIds = new Set<string>();

		for (const device of devices) {
			try {
				const deviceModels = await this.fetchModelsForDevice(device);

				// Add models that we haven't seen before (avoid duplicates)
				for (const model of deviceModels) {
					if (!seenModelIds.has(model.id)) {
						seenModelIds.add(model.id);
						allModels.push(model);
					}
				}
			} catch (error) {
				console.error(`Failed to fetch models for device ${device}:`, error);
				// Continue with other devices if one fails
			}
		}

		return allModels;
	}

	private async fetchModelsForDevice(device: string): Promise<FoundryModel[]> {
		const requestBody = this.buildRequestBody(device);

		try {
			const response = await fetch(FOUNDRY_API_ENDPOINT, {
				method: 'POST',
				headers: {
					'Content-Type': 'application/json',
					Accept: 'application/json'
				},
				body: JSON.stringify(requestBody)
			});

			if (!response.ok) {
				console.error(`API request failed with status: ${response.status} ${response.statusText}`);
				return [];
			}

			const apiData = await response.json();
			const models = this.transformApiResponse(apiData);

			return models;
		} catch (error) {
			console.error('Failed to fetch foundry models:', error);
			return [];
		}
	}

	private buildRequestBody(device: string) {
		const baseFilters = [
			{
				field: 'type',
				operator: 'eq',
				values: ['models']
			},
			{
				field: 'kind',
				operator: 'eq',
				values: ['Versioned']
			},
			{
				field: 'properties/variantInfo/variantMetadata/device',
				operator: 'eq',
				values: [device]
			}
		];

		const requestBody = {
			resourceIds: [
				{
					resourceId: 'azureml',
					entityContainerType: 'Registry'
				}
			],
			indexEntitiesRequest: {
				filters: baseFilters
			}
		};

		return requestBody;
	}

	// eslint-disable-next-line @typescript-eslint/no-explicit-any
	private transformApiResponse(apiData: any): FoundryModel[] {
		// Handle the Azure AI Foundry API response structure
		let entities = [];

		// Check for entities in the response structure
		if (
			apiData?.indexEntitiesResponse?.value &&
			Array.isArray(apiData.indexEntitiesResponse.value)
		) {
			entities = apiData.indexEntitiesResponse.value;
		}
		// Fallback: check if entities are directly in the response
		else if (apiData?.entities && Array.isArray(apiData.entities)) {
			entities = apiData.entities;
		}

		if (entities.length === 0) {
			return [];
		}

		// eslint-disable-next-line @typescript-eslint/no-explicit-any
		return entities
			.map((entity: any) => this.transformSingleModel(entity))
			.filter((model: FoundryModel) => {
				if (this.BLOCKED_MODEL_IDS.has(model.name)) {
					return false;
				}
				if (model.tags.some((tag) => tag.toLowerCase() === 'foundrylocal:hide')) {
					return false;
				}
				// Block any models with "test" in their ID or name
				if (model.id.toLowerCase().includes('test') || model.name.toLowerCase().includes('test')) {
					return false;
				}
				return true;
			});
	}

	// eslint-disable-next-line @typescript-eslint/no-explicit-any
	private transformSingleModel(entity: any): FoundryModel {
		// Extract unique values from devices array
		const deviceSupport: string[] = [];

		// Extract device support from various possible locations in the Azure API response
		if (entity.annotations?.tags?.device) {
			deviceSupport.push(entity.annotations.tags.device);
		}
		if (entity.properties?.variantInfo?.variantMetadata?.device) {
			deviceSupport.push(entity.properties.variantInfo.variantMetadata.device);
		}
		if (entity.properties?.supportedDevices) {
			deviceSupport.push(...entity.properties.supportedDevices);
		}

		// If no device support found, try to infer from model name or ID
		if (deviceSupport.length === 0) {
			const modelId = entity.entityId || '';
			const modelName = entity.name || entity.displayName || '';

			if (modelId.toLowerCase().includes('npu') || modelName.toLowerCase().includes('npu')) {
				deviceSupport.push('npu');
			} else if (modelId.toLowerCase().includes('gpu') || modelName.toLowerCase().includes('gpu')) {
				deviceSupport.push('gpu');
			} else if (modelId.toLowerCase().includes('cpu') || modelName.toLowerCase().includes('cpu')) {
				deviceSupport.push('cpu');
			} else {
				// Default to cpu if we can't determine
				deviceSupport.push('cpu');
			}
		}

		// Remove duplicates manually
		const uniqueDeviceSupport = deviceSupport.filter(
			(device, index) => deviceSupport.indexOf(device) === index
		);

		// Extract name from entityId which looks like: "azureml://registries/.../deepseek-r1-distill-qwen-1.5b-qnn-npu/version/1"
		let modelName = entity.name || entity.displayName || 'Unknown Model';
		if (entity.entityId) {
			const idParts = entity.entityId.split('/');
			const objectIdIndex = idParts.findIndex((part: string) => part === 'objectId');
			if (objectIdIndex >= 0 && objectIdIndex + 1 < idParts.length) {
				modelName = idParts[objectIdIndex + 1];
			}
		}

		// Extract version from entityId
		let version = entity.version || '1.0.0';
		if (entity.entityId) {
			const versionMatch = entity.entityId.match(/\/version\/(\d+(?:\.\d+)*)/);
			if (versionMatch) {
				version = versionMatch[1];
			}
		}

		// Extract tags from annotations
		const tags: string[] = [];
		if (entity.annotations?.tags) {
			Object.entries(entity.annotations.tags).forEach(([key, value]) => {
				if (typeof value === 'string' && value !== 'true' && value !== 'false') {
					tags.push(`${key}:${value}`);
				} else if (key !== 'archived' && key !== 'invisible') {
					tags.push(key);
				}
			});
		}

		// Detect acceleration from model name
		const acceleration = this.detectAcceleration(modelName);

		return {
			id: entity.entityId || entity.id || modelName.toLowerCase().replace(/\s+/g, '-'),
			name: modelName,
			version: version,
			description:
				entity.description ||
				entity.annotations?.description ||
				`${modelName} model for NPU inference`,
			longDescription:
				entity.properties?.readme ||
				entity.properties?.longDescription ||
				entity.annotations?.longDescription,
			deviceSupport: uniqueDeviceSupport,
			tags: tags,
			publisher:
				entity.annotations?.publisher ||
				entity.owner ||
				entity.properties?.author ||
				entity.entityResourceName ||
				'Azure ML',
			acceleration: acceleration,
			lastModified:
				entity.lastModifiedDateTime || entity.properties?.lastModified || new Date().toISOString(),
			createdDate: entity.createdDateTime || entity.properties?.created || new Date().toISOString(),
			downloadCount: entity.properties?.downloadCount || entity.stats?.downloadCount || 0,
			framework:
				entity.annotations?.tags?.framework ||
				entity.properties?.framework ||
				entity.properties?.modelFramework ||
				'ONNX',
			license: entity.properties?.license || entity.license || entity.annotations?.license || 'MIT',
			taskType:
				entity.annotations?.tags?.task ||
				entity.properties?.taskType ||
				entity.properties?.task ||
				'Text Generation',
			modelSize: this.formatModelSize(entity.properties?.modelSize || entity.properties?.size),
			inputFormat: entity.properties?.inputFormat || entity.properties?.inputs?.format,
			outputFormat: entity.properties?.outputFormat || entity.properties?.outputs?.format,
			sampleCode: entity.properties?.sampleCode,
			documentation: entity.properties?.documentation || entity.properties?.readme,
			githubUrl: entity.properties?.githubUrl || entity.properties?.sourceUrl,
			paperUrl: entity.properties?.paperUrl || entity.properties?.paper,
			demoUrl: entity.properties?.demoUrl || entity.properties?.demo,
			benchmarks: this.extractBenchmarks(entity.properties?.benchmarks),
			requirements: entity.properties?.requirements || entity.properties?.dependencies || [],
			compatibleVersions: entity.properties?.compatibleVersions || []
		};
	}

	private extractBenchmarks(
		benchmarkData: unknown
	): Array<{ metric: string; value: string; device: string }> {
		if (!benchmarkData) return [];

		if (Array.isArray(benchmarkData)) {
			return benchmarkData.map((b: Record<string, unknown>) => ({
				metric: (b.metric as string) || (b.name as string) || 'Unknown',
				value: (b.value as number | string)?.toString() || 'N/A',
				device: (b.device as string) || 'npu'
			}));
		}

		// Handle object format
		if (typeof benchmarkData === 'object') {
			const benchmarks: Array<{ metric: string; value: string; device: string }> = [];
			const data = benchmarkData as Record<string, unknown>;
			for (const metric in data) {
				if (Object.prototype.hasOwnProperty.call(data, metric)) {
					benchmarks.push({
						metric,
						value: (data[metric] as number | string)?.toString() || 'N/A',
						device: 'npu'
					});
				}
			}
			return benchmarks;
		}

		return [];
	}

	private formatModelSize(size: unknown): string {
		if (!size) return 'Unknown';

		if (typeof size === 'number') {
			// Convert bytes to human readable format
			const units = ['B', 'KB', 'MB', 'GB', 'TB'];
			let index = 0;
			let sizeNum = size;

			while (sizeNum >= 1024 && index < units.length - 1) {
				sizeNum /= 1024;
				index++;
			}

			return `${sizeNum.toFixed(1)} ${units[index]}`;
		}

		return size.toString();
	}

	private applyClientSideProcessing(
		models: FoundryModel[],
		filters: ApiFilters,
		sortOptions: ApiSortOptions
	): FoundryModel[] {
		let filteredModels = [...models];

		// Apply device filter
		if (filters.device) {
			const deviceFilter = filters.device;
			filteredModels = filteredModels.filter((model) => model.deviceSupport.includes(deviceFilter));
		}

		// Apply family filter (searches in model name/alias)
		if (filters.family) {
			const familyLower = filters.family.toLowerCase();
			filteredModels = filteredModels.filter((model) => {
				const nameMatch = model.name.toLowerCase().includes(familyLower);
				return nameMatch;
			});
		}

		// Apply search filter
		if (filters.searchTerm) {
			const searchLower = filters.searchTerm.toLowerCase();
			filteredModels = filteredModels.filter((model) => {
				const nameMatch = model.name.toLowerCase().indexOf(searchLower) >= 0;
				const descMatch = model.description.toLowerCase().indexOf(searchLower) >= 0;
				const tagMatch = model.tags.some(
					(tag: string) => tag.toLowerCase().indexOf(searchLower) >= 0
				);
				const publisherMatch = model.publisher.toLowerCase().indexOf(searchLower) >= 0;
				return nameMatch || descMatch || tagMatch || publisherMatch;
			});
		}

		// Apply acceleration filter
		if (filters.acceleration) {
			filteredModels = filteredModels.filter(
				(model) => model.acceleration === filters.acceleration
			);
		}

		// Sort models
		filteredModels.sort((a, b) => {
			const aVal: unknown = a[sortOptions.sortBy as keyof FoundryModel];
			const bVal: unknown = b[sortOptions.sortBy as keyof FoundryModel];

			// Handle special sorting cases
			if (sortOptions.sortBy === 'lastModified' || sortOptions.sortBy === 'createdDate') {
				const aDate = new Date(aVal as string | number | Date);
				const bDate = new Date(bVal as string | number | Date);
				if (sortOptions.sortOrder === 'asc') {
					return aDate.getTime() - bDate.getTime();
				} else {
					return bDate.getTime() - aDate.getTime();
				}
			} else if (sortOptions.sortBy === 'downloadCount') {
				const aNum = Number(aVal) || 0;
				const bNum = Number(bVal) || 0;
				if (sortOptions.sortOrder === 'asc') {
					return aNum - bNum;
				} else {
					return bNum - aNum;
				}
			} else {
				// String comparison
				const aStr = String(aVal || '').toLowerCase();
				const bStr = String(bVal || '').toLowerCase();

				if (sortOptions.sortOrder === 'asc') {
					return aStr.localeCompare(bStr);
				} else {
					return bStr.localeCompare(aStr);
				}
			}
		});

		return filteredModels;
	}

	// Helper function to extract alias from model name
	private extractAlias(modelName: string): string {
		// Remove device-specific and acceleration-specific suffixes
		const suffixPatterns = [
			// Acceleration + device combinations
			'-qnn-npu',
			'-vitis-gpu',
			'-vitis-cpu',
			'-vitis-npu',
			'-openvino-npu',
			'-openvino-cpu',
			'-openvino-gpu',
			'-trt-rtx-gpu',
			'-tensorrt-gpu',
			// Device-only suffixes
			'-cuda-gpu',
			'-generic-gpu',
			'-generic-cpu',
			'-cuda',
			'-gpu',
			'-cpu',
			'-npu',
			'-fpga',
			'-asic',
			// Acceleration-only suffixes (less common but just in case)
			'-qnn',
			'-vitis',
			'-openvino',
			'-trt-rtx',
			'-tensorrt'
		];

		let alias = modelName.toLowerCase();

		// Remove suffixes - check longer patterns first
		suffixPatterns.sort((a, b) => b.length - a.length);
		
		for (const suffix of suffixPatterns) {
			if (alias.endsWith(suffix)) {
				alias = alias.slice(0, -suffix.length);
				break;
			}
		}

		return alias;
	}

	// Helper function to create display name from alias
	private createDisplayName(alias: string): string {
		// Convert kebab-case to more readable format
		return alias
			.split('-')
			.map((word) => word.charAt(0).toUpperCase() + word.slice(1))
			.join(' ');
	}

	// Group models by alias and return grouped models
	async fetchGroupedModels(
		filters: ApiFilters = {},
		sortOptions: ApiSortOptions = { sortBy: 'name', sortOrder: 'asc' }
	): Promise<GroupedFoundryModel[]> {
		// First get all individual models
		const allModels = await this.fetchModels(filters, sortOptions);

		// Group models by alias
		const modelGroups = new Map<string, FoundryModel[]>();

		for (const model of allModels) {
			const alias = this.extractAlias(model.name);
			if (!modelGroups.has(alias)) {
				modelGroups.set(alias, []);
			}
			const group = modelGroups.get(alias);
			if (group) {
				group.push(model);
			}
		}

		// Convert groups to GroupedFoundryModel objects
		const groupedModels: GroupedFoundryModel[] = [];

		for (const [alias, variants] of modelGroups) {
			// Sort variants to get the primary one (usually the first alphabetically)
			variants.sort((a, b) => a.name.localeCompare(b.name));
			const primaryModel = variants[0];

			// Combine device support from all variants
			const deviceSupport = [...new Set(variants.flatMap((v) => v.deviceSupport))].sort();

			// Combine tags from all variants
			const tags = [...new Set(variants.flatMap((v) => v.tags))].sort();

			// Sum download counts
			const totalDownloads = variants.reduce((sum, v) => sum + (v.downloadCount || 0), 0);

			// Get latest modification date
			const latestModified = variants.reduce((latest, v) => {
				const vDate = new Date(v.lastModified);
				const latestDate = new Date(latest);
				return vDate > latestDate ? v.lastModified : latest;
			}, variants[0].lastModified);

			// Get earliest creation date
			const earliestCreated = variants.reduce((earliest, v) => {
				const vDate = new Date(v.createdDate);
				const earliestDate = new Date(earliest);
				return vDate < earliestDate ? v.createdDate : earliest;
			}, variants[0].createdDate);

			// Get latest version
			const latestVersion = variants.reduce((latest, v) => {
				// Simple version comparison - you might want to improve this
				return v.version > latest ? v.version : latest;
			}, variants[0].version);

			// Get all accelerations from variants (prefer the first one found for the group)
			const accelerations = variants
				.map((v) => v.acceleration)
				.filter((a): a is string => !!a);
			const groupAcceleration = accelerations.length > 0 ? accelerations[0] : primaryModel.acceleration;

			const groupedModel: GroupedFoundryModel = {
				alias,
				displayName: this.createDisplayName(alias),
				description: primaryModel.description,
				longDescription: primaryModel.longDescription,
				deviceSupport,
				tags,
				publisher: primaryModel.publisher,
				acceleration: groupAcceleration,
				lastModified: latestModified,
				createdDate: earliestCreated,
				downloadCount: totalDownloads,
				framework: primaryModel.framework,
				license: primaryModel.license,
				taskType: primaryModel.taskType,
				modelSize: primaryModel.modelSize,
				variants,
				availableDevices: deviceSupport,
				totalDownloads,
				latestVersion,
				documentation: primaryModel.documentation,
				githubUrl: primaryModel.githubUrl,
				paperUrl: primaryModel.paperUrl,
				demoUrl: primaryModel.demoUrl,
				benchmarks: primaryModel.benchmarks,
				requirements: primaryModel.requirements,
				compatibleVersions: primaryModel.compatibleVersions
			};

			groupedModels.push(groupedModel);
		}

		// Apply sorting to grouped models
		groupedModels.sort((a, b) => {
			let aVal: string | number = '';
			let bVal: string | number = '';

			// Type-safe property access
			switch (sortOptions.sortBy) {
				case 'alias':
					aVal = a.alias;
					bVal = b.alias;
					break;
				case 'displayName':
					aVal = a.displayName;
					bVal = b.displayName;
					break;
				case 'description':
					aVal = a.description;
					bVal = b.description;
					break;
				case 'publisher':
					aVal = a.publisher;
					bVal = b.publisher;
					break;
				case 'totalDownloads':
				case 'downloadCount':
					aVal = a.totalDownloads || 0;
					bVal = b.totalDownloads || 0;
					break;
				case 'lastModified':
					aVal = a.lastModified;
					bVal = b.lastModified;
					break;
				case 'createdDate':
					aVal = a.createdDate;
					bVal = b.createdDate;
					break;
				default:
					aVal = a.displayName;
					bVal = b.displayName;
			}

			if (typeof aVal === 'number' && typeof bVal === 'number') {
				return sortOptions.sortOrder === 'asc' ? aVal - bVal : bVal - aVal;
			} else {
				const aStr = String(aVal).toLowerCase();
				const bStr = String(bVal).toLowerCase();
				return sortOptions.sortOrder === 'asc'
					? aStr.localeCompare(bStr)
					: bStr.localeCompare(aStr);
			}
		});

		return groupedModels;
	}

	async fetchModelById(modelId: string): Promise<FoundryModel | null> {
		try {
			const response = await fetch(`${FOUNDRY_API_ENDPOINT}?id=${encodeURIComponent(modelId)}`);

			if (!response.ok) {
				console.error(`Failed to fetch model ${modelId}: ${response.status}`);
				return null;
			}

			const apiData = await response.json();
			const models = this.transformApiResponse(apiData);

			return models.length > 0 ? models[0] : null;
		} catch (error) {
			console.error(`Error fetching model ${modelId}:`, error);
			return null;
		}
	}
}

export const foundryModelService = new FoundryModelService();
