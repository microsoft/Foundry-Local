<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { foundryModelService } from './service';
	import type { GroupedFoundryModel } from './types';
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import { Input } from '$lib/components/ui/input';
	import { Label } from '$lib/components/ui/label';
	import Nav from '$lib/components/home/nav.svelte';
	import Footer from '$lib/components/home/footer.svelte';
	import {
		Search,
		Filter,
		Download,
		Calendar,
		Package,
		RefreshCw,
		Copy,
		Check,
		ExternalLink,
		Info
	} from 'lucide-svelte';
	import * as Card from '$lib/components/ui/card';
	import * as Dialog from '$lib/components/ui/dialog';
	import { toast } from 'svelte-sonner';
	import { animate } from '$lib/utils/animations';

	// Debounce utility for search
	let searchDebounceTimer: ReturnType<typeof setTimeout> | null = null;
	let debouncedSearchTerm = '';

	// State
	let allModels: GroupedFoundryModel[] = [];
	let filteredModels: GroupedFoundryModel[] = [];
	let loading = false;
	let error = '';
	let copiedModelId: string | null = null;
	
	// Modal state
	let selectedModel: GroupedFoundryModel | null = null;
	let isModalOpen = false;

	// Filter state
	let searchTerm = '';
	let selectedDevices: string[] = [];
	let selectedFamily = '';
	let selectedAcceleration = '';

	// Available filter options
	let availableDevices: string[] = [];
	let availableFamilies: string[] = ['deepseek', 'mistral', 'qwen', 'phi'];
	let availableAccelerations: string[] = [];

	// Toggle device selection
	function toggleDevice(device: string) {
		if (selectedDevices.includes(device)) {
			selectedDevices = selectedDevices.filter((d) => d !== device);
		} else {
			selectedDevices = [...selectedDevices, device];
		}
	}

	// Sort options
	let sortBy = 'name';
	let sortOrder: 'asc' | 'desc' = 'asc';

	// Pagination
	let currentPage = 1;
	let itemsPerPage = 12;
	$: totalPages = Math.ceil(filteredModels.length / itemsPerPage);
	$: paginatedModels = filteredModels.slice(
		(currentPage - 1) * itemsPerPage,
		currentPage * itemsPerPage
	);

	// Fetch all models from API once
	async function fetchAllModels() {
		loading = true;
		error = '';

		try {
			const fetchedModels = await foundryModelService.fetchGroupedModels(
				{},
				{ sortBy: 'name', sortOrder: 'asc' }
			);

			allModels = fetchedModels;
			updateFilterOptions();
		} catch (err: any) {
			console.error('Error fetching models:', err);
			error = `Failed to fetch models: ${err.message}`;
		} finally {
			loading = false;
		}
	}

	// Refresh models
	async function refreshModels() {
		foundryModelService.clearCache();
		await fetchAllModels();
		toast.success('Models refreshed successfully');
	}

	function updateFilterOptions() {
		availableDevices = [...new Set(allModels.flatMap((m) => m.deviceSupport))].sort();
		
		// Collect accelerations from both the group and all variants
		const accelerations = new Set<string>();
		allModels.forEach((model) => {
			if (model.acceleration) {
				accelerations.add(model.acceleration);
			}
			// Also check variants for their accelerations
			if (model.variants) {
				model.variants.forEach((variant) => {
					if (variant.acceleration) {
						accelerations.add(variant.acceleration);
					}
				});
			}
		});
		
		availableAccelerations = [...accelerations].sort();
	}

	function applyFilters() {
		filteredModels = allModels.filter((model) => {
			const searchLower = debouncedSearchTerm.toLowerCase();
			const matchesSearch =
				!debouncedSearchTerm ||
				model.displayName.toLowerCase().includes(searchLower) ||
				model.alias.toLowerCase().includes(searchLower) ||
				model.description.toLowerCase().includes(searchLower) ||
				model.tags.some((tag) => tag.toLowerCase().includes(searchLower)) ||
				// Search in variant names to match acceleration types like 'qnn', 'vitis', 'cuda', etc.
				(model.variants && model.variants.some((v) => v.name.toLowerCase().includes(searchLower))) ||
				// Search in acceleration display name
				(model.acceleration && foundryModelService.getAccelerationDisplayName(model.acceleration).toLowerCase().includes(searchLower));

			const matchesDevice =
				selectedDevices.length === 0 ||
				selectedDevices.some((device) => model.deviceSupport.includes(device));
			const matchesFamily =
				!selectedFamily ||
				model.displayName.toLowerCase().includes(selectedFamily.toLowerCase()) ||
				model.alias.toLowerCase().includes(selectedFamily.toLowerCase());
			
			// Check if model or ANY of its variants have the selected acceleration
			const matchesAcceleration =
				!selectedAcceleration || 
				model.acceleration === selectedAcceleration ||
				(model.variants && model.variants.some((v) => v.acceleration === selectedAcceleration));

			return matchesSearch && matchesDevice && matchesFamily && matchesAcceleration;
		});

		// Apply sorting
		filteredModels.sort((a, b) => {
			let aVal: any;
			let bVal: any;

			switch (sortBy) {
				case 'displayName':
				case 'name':
					aVal = a.displayName;
					bVal = b.displayName;
					break;
				case 'totalDownloads':
				case 'downloadCount':
					aVal = a.totalDownloads || 0;
					bVal = b.totalDownloads || 0;
					break;
				default:
					aVal = (a as any)[sortBy];
					bVal = (b as any)[sortBy];
			}

			if (sortBy === 'lastModified') {
				aVal = new Date(aVal);
				bVal = new Date(bVal);
			} else if (sortBy === 'downloadCount') {
				aVal = aVal || 0;
				bVal = bVal || 0;
			} else if (typeof aVal === 'string') {
				aVal = aVal.toLowerCase();
				bVal = bVal.toLowerCase();
			}

			if (sortOrder === 'asc') {
				return aVal > bVal ? 1 : -1;
			} else {
				return aVal < bVal ? 1 : -1;
			}
		});

		currentPage = 1;
	}

	function clearFilters() {
		searchTerm = '';
		debouncedSearchTerm = '';
		selectedDevices = [];
		selectedFamily = '';
		selectedAcceleration = '';
		sortBy = 'name';
		sortOrder = 'asc';
		currentPage = 1;
	}

	function formatDate(dateString: string): string {
		return new Date(dateString).toLocaleDateString('en-US', {
			year: 'numeric',
			month: 'short',
			day: 'numeric'
		});
	}

	function getDeviceIcon(device: string): string {
		const icons: Record<string, string> = {
			npu: '🧠',
			gpu: '🎮',
			cpu: '💻'
		};
		return icons[device.toLowerCase()] || '🔧';
	}

	// Get unique variants by model name to avoid duplicate buttons
	function getUniqueVariants(model: GroupedFoundryModel) {
		if (!model.variants || model.variants.length === 0) return [];

		const uniqueMap = new Map();
		for (const variant of model.variants) {
			if (!uniqueMap.has(variant.name)) {
				uniqueMap.set(variant.name, variant);
			}
		}
		return Array.from(uniqueMap.values());
	}

	// Sort variants by device priority: CPU -> GPU -> NPU
	function sortVariantsByDevice(variants: any[]) {
		const devicePriority: Record<string, number> = {
			cpu: 1,
			gpu: 2,
			npu: 3
		};

		return [...variants].sort((a, b) => {
			// Get the primary device for each variant (first in deviceSupport array)
			const aDevice = a.deviceSupport[0]?.toLowerCase() || 'zzz';
			const bDevice = b.deviceSupport[0]?.toLowerCase() || 'zzz';

			const aPriority = devicePriority[aDevice] || 99;
			const bPriority = devicePriority[bDevice] || 99;

			return aPriority - bPriority;
		});
	}

	// Generate button label showing device and acceleration (if present)
	function getVariantLabel(variant: any): string {
		const modelName = variant.name.toLowerCase();
		const device = variant.deviceSupport[0]?.toUpperCase() || '';
		
		// Check for CUDA GPU vs Generic GPU
		if (modelName.includes('-cuda-gpu') || modelName.includes('-cuda-')) {
			// Check for acceleration in CUDA GPUs
			if (modelName.includes('-trt-rtx-') || modelName.includes('-tensorrt-')) {
				return `${device} (CUDA + TensorRT)`;
			}
			return `${device} (CUDA)`;
		} else if (modelName.includes('-generic-gpu')) {
			return `${device} (Generic)`;
		}
		
		// Check for acceleration in the model name
		if (modelName.includes('-qnn-')) {
			return `${device} (QNN)`;
		} else if (modelName.includes('-vitis-')) {
			return `${device} (Vitis)`;
		} else if (modelName.includes('-openvino-')) {
			return `${device} (OpenVINO)`;
		} else if (modelName.includes('-trt-rtx-') || modelName.includes('-tensorrt-')) {
			return `${device} (TensorRT)`;
		}
		
		// Check for generic CPU
		if (modelName.includes('-generic-cpu')) {
			return `${device} (Generic)`;
		}
		
		return device;
	}

	async function copyModelId(modelId: string) {
		try {
			await navigator.clipboard.writeText(modelId);
			copiedModelId = modelId;
			toast.success('Model ID copied to clipboard');
			setTimeout(() => {
				copiedModelId = null;
			}, 2000);
		} catch (err) {
			toast.error('Failed to copy to clipboard');
		}
	}

	async function copyRunCommand(modelId: string) {
		try {
			const command = formatModelCommand(modelId);
			await navigator.clipboard.writeText(command);
			copiedModelId = `run-${modelId}`;
			toast.success('Run command copied to clipboard');
			setTimeout(() => {
				copiedModelId = null;
			}, 2000);
		} catch (err) {
			toast.error('Failed to copy to clipboard');
		}
	}

	function openModelDetails(model: GroupedFoundryModel) {
		selectedModel = model;
		isModalOpen = true;
	}

	function closeModal() {
		isModalOpen = false;
		setTimeout(() => {
			selectedModel = null;
		}, 200);
	}

	function formatModelCommand(modelId: string): string {
		return `foundry model run ${modelId}`;
	}

	// Convert markdown to HTML (basic support)
	function renderMarkdown(text: string): string {
		if (!text) return '';
		
		let html = text;
		
		// Headers (must be done before other replacements)
		html = html.replace(/^#### (.*?)$/gm, '<h4 class="text-base font-semibold mt-4 mb-2">$1</h4>');
		html = html.replace(/^### (.*?)$/gm, '<h3 class="text-lg font-semibold mt-5 mb-2">$1</h3>');
		html = html.replace(/^## (.*?)$/gm, '<h2 class="text-xl font-bold mt-6 mb-3">$1</h2>');
		html = html.replace(/^# (.*?)$/gm, '<h1 class="text-2xl font-bold mt-6 mb-3">$1</h1>');
		
		// Process the text line by line to handle lists properly
		const lines = html.split('\n');
		const processed: string[] = [];
		let inList = false;
		let listItems: string[] = [];
		
		for (let i = 0; i < lines.length; i++) {
			const line = lines[i];
			const trimmedLine = line.trim();
			
			// Check if this is a list item (starts with - or *)
			const listMatch = trimmedLine.match(/^[-*]\s+(.+)$/);
			
			if (listMatch) {
				// We're in a list
				if (!inList) {
					inList = true;
					listItems = [];
				}
				listItems.push(listMatch[1]);
			} else {
				// Not a list item
				if (inList) {
					// Close the previous list
					const listHtml = '<ul class="list-disc list-inside mb-3 space-y-1">' + 
						listItems.map(item => `<li>${item}</li>`).join('') + 
						'</ul>';
					processed.push(listHtml);
					inList = false;
					listItems = [];
				}
				processed.push(line);
			}
		}
		
		// Close any remaining list
		if (inList) {
			const listHtml = '<ul class="list-disc list-inside mb-3 space-y-1">' + 
				listItems.map(item => `<li>${item}</li>`).join('') + 
				'</ul>';
			processed.push(listHtml);
		}
		
		html = processed.join('\n');
		
		// Bold (must be before italic to avoid conflicts)
		html = html.replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>');
		
		// Italic (but not list markers)
		html = html.replace(/(?<!^)(?<![-\s])\*([^\*\n]+?)\*/g, '<em>$1</em>');
		
		// Code inline
		html = html.replace(/`(.*?)`/g, '<code>$1</code>');
		
		// Links
		html = html.replace(/\[(.*?)\]\((.*?)\)/g, '<a href="$2" target="_blank" rel="noopener noreferrer" class="text-primary hover:underline">$1</a>');
		
		// Process paragraphs
		html = html.split('\n\n').map(para => {
			const trimmed = para.trim();
			if (trimmed && !trimmed.startsWith('<h') && !trimmed.startsWith('<ul') && !trimmed.startsWith('<ol')) {
				return `<p class="mb-3">${para.replace(/\n/g, '<br>')}</p>`;
			}
			return para;
		}).join('\n');
		
		return html;
	}

	// Reactive statements for automatic filtering
	$: {
		if (searchDebounceTimer) {
			clearTimeout(searchDebounceTimer);
		}
		searchDebounceTimer = setTimeout(() => {
			debouncedSearchTerm = searchTerm;
		}, 300);
	}

	$: {
		if (allModels.length > 0) {
			applyFilters();
		} else {
			filteredModels = [];
		}
		selectedDevices;
		selectedFamily;
		selectedAcceleration;
		debouncedSearchTerm;
		sortBy;
		sortOrder;
	}

	onMount(() => {
		fetchAllModels();
	});

	onDestroy(() => {
		if (searchDebounceTimer) {
			clearTimeout(searchDebounceTimer);
		}
	});

	let description =
		'Discover and explore Foundry local models optimized for various hardware devices including NPUs, GPUs, CPUs, FPGAs and other specialized compute platforms.';
	let keywords =
		'foundry, local models, npu models, gpu models, cpu models, onnx runtime, machine learning models, ai models, hardware optimization';
</script>

<svelte:head>
	<title>Foundry Local Models - Browse AI Models</title>
	<meta name="description" content={description} />
	<meta name="keywords" content={keywords} />
	<meta property="og:title" content="Foundry Local Models" />
	<meta property="og:description" content={description} />
	<meta property="twitter:title" content="Foundry Local Models" />
	<meta property="twitter:description" content={description} />
</svelte:head>

<Nav />

<div class="bg-white dark:bg-neutral-950">
	<!-- Hero Section -->
	<div
		class="relative overflow-hidden bg-gradient-to-br from-blue-50 to-purple-50 dark:from-neutral-900 dark:to-neutral-950"
	>
		<div class="relative mx-auto max-w-[85rem] px-4 pb-6 pt-20 sm:px-6 lg:px-8">
			<div class="mx-auto max-w-3xl text-center">
				<h1 
					use:animate={{ delay: 0, duration: 800, animation: 'slide-up' }}
					class="mb-3 text-3xl font-bold text-gray-800 dark:text-neutral-200 md:text-4xl"
				>
					<span class="text-primary">Foundry Local</span> Models
				</h1>
				<p 
					use:animate={{ delay: 200, duration: 800, animation: 'slide-up' }}
					class="text-base text-gray-600 dark:text-neutral-400"
				>
					Explore AI models optimized for local deployment. Search and filter to find models for
					your NPUs, GPUs, and CPUs.
				</p>
			</div>
		</div>
	</div>

	<!-- Main Content -->
	<main id="main-content" class="mx-auto max-w-[85rem] px-4 py-6 sm:px-6 lg:px-8">
		<!-- Filters Section -->
		<div use:animate={{ delay: 100, duration: 600, animation: 'fade-in' }}>
		<Card.Root class="mb-6">
			<Card.Header>
				<div class="flex items-center justify-between">
					<div>
						<Card.Title class="flex items-center gap-2">
							<Filter class="size-5" />
							Filter & Search Models
						</Card.Title>
						<Card.Description
							>Results update automatically as you type or change filters</Card.Description
						>
					</div>
					<Button variant="ghost" size="sm" onclick={refreshModels} disabled={loading}>
						<RefreshCw class={`mr-1 size-4 ${loading ? 'animate-spin' : ''}`} />
						Refresh
					</Button>
				</div>
			</Card.Header>
			<Card.Content>
				<div class="grid gap-4 md:grid-cols-2 lg:grid-cols-3">
					<!-- Search -->
					<div class="lg:col-span-2">
						<Label for="search">Search Models</Label>
						<div class="relative">
							<Search class="absolute left-3 top-1/2 size-4 -translate-y-1/2 text-gray-400" />
							<Input
								id="search"
								type="text"
								bind:value={searchTerm}
								placeholder="Search by name, description, or tags..."
								class="pl-10"
							/>
						</div>
					</div>

					<!-- Sort -->
					<div>
						<Label>Sort By</Label>
						<div class="flex gap-2">
							<select
								bind:value={sortBy}
								class="flex h-10 w-full rounded-md border border-input bg-background px-3 py-2 text-sm"
							>
								<option value="name">Name</option>
								<option value="lastModified">Last Modified</option>
							</select>
							<select
								bind:value={sortOrder}
								class="flex h-10 rounded-md border border-input bg-background px-3 py-2 text-sm"
							>
								<option value="asc">↑</option>
								<option value="desc">↓</option>
							</select>
						</div>
					</div>

					<!-- Device Filter -->
					<div>
						<Label>Execution Device</Label>
						<div class="flex h-10 gap-2">
							{#each availableDevices as device}
								<Button
									variant={selectedDevices.includes(device) ? 'default' : 'outline'}
									size="sm"
									onclick={() => toggleDevice(device)}
									class="h-full flex-1"
								>
									{getDeviceIcon(device)}
									{device.toUpperCase()}
								</Button>
							{/each}
						</div>
					</div>

					<!-- Family Filter -->
					<div>
						<Label for="family">Model Family</Label>
						<select
							id="family"
							bind:value={selectedFamily}
							class="flex h-10 w-full rounded-md border border-input bg-background px-3 py-2 text-sm"
						>
							<option value="">All Families</option>
							{#each availableFamilies as family}
								<option value={family}>{family.charAt(0).toUpperCase() + family.slice(1)}</option>
							{/each}
						</select>
					</div>

					<!-- Acceleration Filter -->
					<div>
						<Label for="acceleration">Acceleration</Label>
						<select
							id="acceleration"
							bind:value={selectedAcceleration}
							class="flex h-10 w-full rounded-md border border-input bg-background px-3 py-2 text-sm"
						>
							<option value="">All Accelerations</option>
							{#each availableAccelerations as acceleration}
								<option value={acceleration}>
									{foundryModelService.getAccelerationDisplayName(acceleration)}
								</option>
							{/each}
						</select>
					</div>
				</div>

				<!-- Filter Summary -->
				<div class="mt-4 flex items-center justify-between border-t pt-4">
					<div class="text-sm text-gray-600 dark:text-gray-400">
						{#if searchTerm !== debouncedSearchTerm}
							<span class="inline-flex items-center">
								<div
									class="mr-2 size-4 animate-spin rounded-full border-2 border-primary border-t-transparent"
								></div>
								Filtering...
							</span>
						{:else}
							{filteredModels.length} model{filteredModels.length !== 1 ? 's' : ''} found
						{/if}
					</div>
					{#if searchTerm || selectedDevices.length > 0 || selectedFamily || selectedAcceleration}
						<Button variant="outline" size="sm" onclick={clearFilters}>Clear Filters</Button>
					{/if}
				</div>
			</Card.Content>
		</Card.Root>
		</div>

		<!-- Loading State -->
		{#if loading}
			<div class="flex flex-col items-center justify-center py-20">
				<div
					class="mb-4 size-12 animate-spin rounded-full border-4 border-primary border-t-transparent"
				></div>
				<p class="text-lg text-gray-600 dark:text-gray-400">Loading foundry models...</p>
			</div>
		{/if}

		<!-- Error State -->
		{#if error}
			<Card.Root class="border-red-200 bg-red-50 dark:border-red-900 dark:bg-red-950">
				<Card.Content class="pt-6">
					<div class="flex items-start gap-4">
						<div class="rounded-full bg-red-100 p-2 dark:bg-red-900">
							<svg
								class="size-6 text-red-600 dark:text-red-400"
								fill="none"
								stroke="currentColor"
								viewBox="0 0 24 24"
							>
								<path
									stroke-linecap="round"
									stroke-linejoin="round"
									stroke-width="2"
									d="M6 18L18 6M6 6l12 12"
								/>
							</svg>
						</div>
						<div class="flex-1">
							<h3 class="font-semibold text-red-900 dark:text-red-100">Error Loading Models</h3>
							<p class="mt-1 text-sm text-red-700 dark:text-red-300">{error}</p>
							<Button variant="outline" size="sm" onclick={refreshModels} class="mt-4">
								Try Again
							</Button>
						</div>
					</div>
				</Card.Content>
			</Card.Root>
		{/if}

		<!-- Models Grid -->
		{#if !loading && !error}
			{#if paginatedModels.length > 0}
				<div class="grid gap-6 sm:grid-cols-2 lg:grid-cols-3 auto-rows-fr">
					{#each paginatedModels as model (model.alias)}
						<div use:animate={{ delay: 0, duration: 600, animation: 'fade-in', once: true }} class="flex">
						<Card.Root 
							class="flex flex-col flex-1 cursor-pointer transition-all duration-300 hover:-translate-y-1 hover:shadow-xl hover:border-primary/50" 
							onclick={() => openModelDetails(model)}
						>
							<Card.Header>
								<Card.Title class="line-clamp-1">
									{model.displayName}
								</Card.Title>
								<Card.Description class="text-xs">{model.publisher}</Card.Description>
							</Card.Header>
							<Card.Content class="flex flex-col flex-1">
								<p class="mb-4 line-clamp-3 text-sm text-gray-600 dark:text-gray-400">
									{model.description}
								</p>

								<!-- Badges with Date, Version, Task Type, and License -->
								<div class="mb-4 flex flex-row items-center gap-2 flex-wrap">
									<!-- Date as first badge -->
									<div class="flex items-center gap-1 text-xs text-gray-500">
										<Calendar class="size-3" />
										<span class="whitespace-nowrap">{new Date(model.lastModified).toLocaleDateString('en-US', { month: 'short', year: 'numeric' })}</span>
									</div>
									<Badge variant="secondary" class="text-xs">v{model.latestVersion}</Badge>
									{#if model.taskType}
										<Badge variant="secondary" class="text-xs">{model.taskType}</Badge>
									{/if}
									{#if model.license}
										<Badge variant="outline" class="flex items-center gap-1 text-xs">
											<svg class="size-3" fill="currentColor" viewBox="0 0 20 20">
												<path d="M9 2a1 1 0 000 2h2a1 1 0 100-2H9z" />
												<path
													fill-rule="evenodd"
													d="M4 5a2 2 0 012-2 3 3 0 003 3h2a3 3 0 003-3 2 2 0 012 2v11a2 2 0 01-2 2H6a2 2 0 01-2-2V5zm3 4a1 1 0 000 2h.01a1 1 0 100-2H7zm3 0a1 1 0 000 2h3a1 1 0 100-2h-3zm-3 4a1 1 0 100 2h.01a1 1 0 100-2H7zm3 0a1 1 0 100 2h3a1 1 0 100-2h-3z"
													clip-rule="evenodd"
												/>
											</svg>
											{model.license}
										</Badge>
									{/if}
								</div>

								<!-- Copy Run Command Section -->
								<div class="mt-auto border-t pt-4">
									{#if model.variants && model.variants.length > 0}
										{@const uniqueVariants = getUniqueVariants(model)}
										{@const sortedVariants = sortVariantsByDevice(uniqueVariants)}
										<div class="space-y-2">
											<div class="text-xs font-medium text-gray-600 dark:text-gray-400">
												Copy Run Command:
											</div>
											<div class="grid grid-cols-2 gap-2">
												{#each sortedVariants as variant}
													{@const primaryDevice = variant.deviceSupport[0] || ''}
													{@const variantLabel = getVariantLabel(variant)}
													<Button
														variant="outline"
														size="sm"
														onclick={(e) => {
															e.stopPropagation();
															copyRunCommand(variant.name);
														}}
														class="h-8 gap-1.5 px-3 text-xs justify-start"
													>
														{#if copiedModelId === variant.name}
															<Check class="size-3 text-green-500 shrink-0" />
															<span class="truncate">Copied!</span>
														{:else}
															<span class="shrink-0">{getDeviceIcon(primaryDevice)}</span>
															<span class="truncate">{variantLabel}</span>
														{/if}
													</Button>
												{/each}
											</div>
										</div>
									{/if}
								</div>
							</Card.Content>
						</Card.Root>
						</div>
					{/each}
				</div>

				<!-- Pagination -->
				{#if totalPages > 1}
					<div class="mt-8 flex justify-center gap-2">
						<Button
							variant="outline"
							size="sm"
							disabled={currentPage === 1}
							onclick={() => (currentPage = Math.max(1, currentPage - 1))}
						>
							Previous
						</Button>

						<div class="flex items-center gap-2">
							{#each Array(totalPages).fill(0) as _, i}
								{@const page = i + 1}
								{#if page === 1 || page === totalPages || (page >= currentPage - 2 && page <= currentPage + 2)}
									<Button
										variant={currentPage === page ? 'default' : 'outline'}
										size="sm"
										onclick={() => (currentPage = page)}
									>
										{page}
									</Button>
								{:else if page === currentPage - 3 || page === currentPage + 3}
									<span class="px-2">...</span>
								{/if}
							{/each}
						</div>

						<Button
							variant="outline"
							size="sm"
							disabled={currentPage === totalPages}
							onclick={() => (currentPage = Math.min(totalPages, currentPage + 1))}
						>
							Next
						</Button>
					</div>
				{/if}
			{:else}
				<Card.Root>
					<Card.Content class="py-12 text-center">
						<div class="mb-4 text-6xl">🔍</div>
						<h3 class="mb-2 text-xl font-semibold">No models found</h3>
						<p class="mb-4 text-gray-600 dark:text-gray-400">
							Try adjusting your search criteria or clearing the filters.
						</p>
						<Button onclick={clearFilters}>Clear All Filters</Button>
					</Card.Content>
				</Card.Root>
			{/if}
		{/if}
	</main>
</div>

<!-- Model Details Modal -->
<Dialog.Root bind:open={isModalOpen}>
	<Dialog.Content class="max-w-4xl max-h-[90vh] overflow-y-auto">
		{#if selectedModel}
			<Dialog.Header>
				<Dialog.Title class="text-2xl font-bold flex items-center gap-3">
					<span>{selectedModel.displayName}</span>
					<Badge variant="secondary" class="text-xs">v{selectedModel.latestVersion}</Badge>
				</Dialog.Title>
				<Dialog.Description class="text-sm text-muted-foreground">
					{selectedModel.publisher}
				</Dialog.Description>
			</Dialog.Header>

			<div class="mt-6 space-y-6">
				<!-- Stats Section -->
				<div class="grid grid-cols-2 gap-4 sm:grid-cols-3">
					<div class="rounded-lg border bg-card p-4">
						<div class="text-2xl font-bold text-primary">{selectedModel.variants.length}</div>
						<div class="text-xs text-muted-foreground">Variants</div>
					</div>
					<div class="rounded-lg border bg-card p-4">
						<div class="text-2xl font-bold text-primary">{formatDate(selectedModel.lastModified)}</div>
						<div class="text-xs text-muted-foreground">Updated</div>
					</div>
					<div class="rounded-lg border bg-card p-4">
						<div class="flex flex-wrap gap-2">
							{#each selectedModel.deviceSupport as device}
								<div class="inline-flex items-center gap-1 rounded-md bg-primary/10 px-2.5 py-1 text-sm font-medium">
									<span class="text-base">{getDeviceIcon(device)}</span>
									<span class="text-primary">{device.toUpperCase()}</span>
								</div>
							{/each}
						</div>
						<div class="mt-2 text-xs text-muted-foreground">Supported Devices</div>
					</div>
				</div>

				<!-- Description -->
				<div>
					<h3 class="mb-2 text-lg font-semibold">Description</h3>
					<div class="text-sm text-muted-foreground leading-relaxed prose prose-sm dark:prose-invert max-w-none">
						{@html renderMarkdown(selectedModel.longDescription || selectedModel.description)}
					</div>
				</div>

				<!-- Model Details -->
				<div>
					<h3 class="mb-3 text-lg font-semibold">Model Information</h3>
					<div class="grid gap-3 sm:grid-cols-2">
						{#if selectedModel.taskType}
							<div class="flex items-start gap-3 rounded-lg border bg-card/50 p-3">
								<Package class="mt-0.5 size-4 text-primary" />
								<div>
									<div class="text-xs font-medium text-muted-foreground">Task Type</div>
									<div class="text-sm font-medium">{selectedModel.taskType}</div>
								</div>
							</div>
						{/if}
						{#if selectedModel.license}
							<div class="flex items-start gap-3 rounded-lg border bg-card/50 p-3">
								<svg class="mt-0.5 size-4 text-primary" fill="currentColor" viewBox="0 0 20 20">
									<path d="M9 2a1 1 0 000 2h2a1 1 0 100-2H9z" />
									<path
										fill-rule="evenodd"
										d="M4 5a2 2 0 012-2 3 3 0 003 3h2a3 3 0 003-3 2 2 0 012 2v11a2 2 0 01-2 2H6a2 2 0 01-2-2V5zm3 4a1 1 0 000 2h.01a1 1 0 100-2H7zm3 0a1 1 0 000 2h3a1 1 0 100-2h-3zm-3 4a1 1 0 100 2h.01a1 1 0 100-2H7zm3 0a1 1 0 100 2h3a1 1 0 100-2h-3z"
										clip-rule="evenodd"
									/>
								</svg>
								<div>
									<div class="text-xs font-medium text-muted-foreground">License</div>
									<div class="text-sm font-medium">{selectedModel.license}</div>
								</div>
							</div>
						{/if}
						{#if selectedModel.acceleration}
							<div class="flex items-start gap-3 rounded-lg border bg-card/50 p-3">
								<svg class="mt-0.5 size-4 text-primary" fill="currentColor" viewBox="0 0 20 20">
									<path fill-rule="evenodd" d="M11.3 1.046A1 1 0 0112 2v5h4a1 1 0 01.82 1.573l-7 10A1 1 0 018 18v-5H4a1 1 0 01-.82-1.573l7-10a1 1 0 011.12-.38z" clip-rule="evenodd" />
								</svg>
								<div>
									<div class="text-xs font-medium text-muted-foreground">Acceleration</div>
									<div class="text-sm font-medium">{foundryModelService.getAccelerationDisplayName(selectedModel.acceleration)}</div>
								</div>
							</div>
						{/if}
					</div>
				</div>

				<!-- Available Variants -->
				<div>
					<h3 class="mb-3 text-lg font-semibold">Available Model Variants</h3>
					<div class="space-y-3">
						{#each getUniqueVariants(selectedModel) as variant}
							<div class="rounded-lg border bg-card p-4 transition-all hover:border-primary/50">
								<div class="mb-3 flex items-start justify-between">
									<div class="flex-1">
										<div class="font-mono text-sm font-medium">{variant.name}</div>
										<div class="mt-1 flex items-center gap-2 text-xs text-muted-foreground">
											<span>Device:</span>
											{#each variant.deviceSupport as device}
												<Badge variant="secondary" class="text-xs">
													{getDeviceIcon(device)} {getVariantLabel(variant)}
												</Badge>
											{/each}
										</div>
									</div>
									<Button
										variant="outline"
										size="sm"
										onclick={(e) => {
											e.stopPropagation();
											copyModelId(variant.name);
										}}
										class="gap-2"
									>
										{#if copiedModelId === variant.name}
											<Check class="size-4 text-green-500" />
											Copied
										{:else}
											<Copy class="size-4" />
											Copy ID
										{/if}
									</Button>
								</div>
								
								<!-- Command to run -->
								<div class="rounded-md bg-muted/50 p-3">
									<div class="mb-2 flex items-center justify-between">
										<div class="text-xs font-medium text-muted-foreground">Run Command:</div>
										<Button
											variant="ghost"
											size="sm"
											onclick={(e) => {
												e.stopPropagation();
												copyRunCommand(variant.name);
											}}
											class="h-6 gap-1.5 px-2 text-xs"
										>
											{#if copiedModelId === `run-${variant.name}`}
												<Check class="size-3 text-green-500" />
												Copied
											{:else}
												<Copy class="size-3" />
												Copy
											{/if}
										</Button>
									</div>
									<code class="text-xs font-mono">{formatModelCommand(variant.name)}</code>
								</div>
							</div>
						{/each}
					</div>
				</div>

				<!-- Links Section -->
				{#if selectedModel.githubUrl || selectedModel.paperUrl || selectedModel.demoUrl || selectedModel.documentation}
					<div>
						<h3 class="mb-3 text-lg font-semibold">Resources</h3>
						<div class="flex flex-wrap gap-2">
							{#if selectedModel.githubUrl}
								<Button variant="outline" size="sm" onclick={() => selectedModel && window.open(selectedModel.githubUrl, '_blank')}>
									<ExternalLink class="mr-2 size-4" />
									GitHub
								</Button>
							{/if}
							{#if selectedModel.paperUrl}
								<Button variant="outline" size="sm" onclick={() => selectedModel && window.open(selectedModel.paperUrl, '_blank')}>
									<ExternalLink class="mr-2 size-4" />
									Paper
								</Button>
							{/if}
							{#if selectedModel.demoUrl}
								<Button variant="outline" size="sm" onclick={() => selectedModel && window.open(selectedModel.demoUrl, '_blank')}>
									<ExternalLink class="mr-2 size-4" />
									Demo
								</Button>
							{/if}
							{#if selectedModel.documentation}
								<Button variant="outline" size="sm" onclick={() => selectedModel && window.open(selectedModel.documentation, '_blank')}>
									<ExternalLink class="mr-2 size-4" />
									Documentation
								</Button>
							{/if}
						</div>
					</div>
				{/if}

				<!-- Tags -->
				{#if selectedModel.tags && selectedModel.tags.length > 0}
					<div>
						<h3 class="mb-2 text-lg font-semibold">Tags</h3>
						<div class="flex flex-wrap gap-2">
							{#each selectedModel.tags as tag}
								<Badge variant="outline" class="text-xs">{tag}</Badge>
							{/each}
						</div>
					</div>
				{/if}
			</div>
		{/if}
	</Dialog.Content>
</Dialog.Root>

<Footer />

<style>
	:global(.line-clamp-1) {
		display: -webkit-box;
		-webkit-line-clamp: 1;
		line-clamp: 1;
		-webkit-box-orient: vertical;
		overflow: hidden;
	}

	:global(.line-clamp-3) {
		display: -webkit-box;
		-webkit-line-clamp: 3;
		line-clamp: 3;
		-webkit-box-orient: vertical;
		overflow: hidden;
	}

	.prose :global(code) {
		background-color: hsl(var(--muted));
		padding: 0.125rem 0.25rem;
		border-radius: 0.25rem;
		font-size: 0.875em;
		font-family: ui-monospace, SFMono-Regular, 'SF Mono', Menlo, Consolas, 'Liberation Mono', monospace;
	}

	.prose :global(a) {
		color: hsl(var(--primary));
		text-decoration: none;
	}

	.prose :global(a:hover) {
		text-decoration: underline;
	}

	.prose :global(strong) {
		font-weight: 600;
		color: hsl(var(--foreground));
	}

	.prose :global(h1),
	.prose :global(h2),
	.prose :global(h3),
	.prose :global(h4) {
		color: hsl(var(--foreground));
		line-height: 1.3;
	}

	.prose :global(h1:first-child),
	.prose :global(h2:first-child),
	.prose :global(h3:first-child),
	.prose :global(h4:first-child) {
		margin-top: 0;
	}

	.prose :global(p) {
		margin-bottom: 0.75rem;
		line-height: 1.6;
	}

	.prose :global(p:last-child) {
		margin-bottom: 0;
	}

	.prose :global(ul) {
		margin-bottom: 0.75rem;
		padding-left: 0;
	}

	.prose :global(li) {
		line-height: 1.6;
		margin-bottom: 0.25rem;
		color: hsl(var(--muted-foreground));
	}

	.prose :global(li):last-child {
		margin-bottom: 0;
	}
</style>
