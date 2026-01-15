<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { foundryModelService } from './service';
	import type { GroupedFoundryModel } from './types';
	import Nav from '$lib/components/home/nav.svelte';
	import Footer from '$lib/components/home/footer.svelte';
	import * as Card from '$lib/components/ui/card';
	import * as Tooltip from '$lib/components/ui/tooltip';
	import { toast } from 'svelte-sonner';
	import { ModelFilters, ModelGrid, ModelDetailsModal } from './components';

	// Debounce timer for search
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
	let sortBy = 'lastModified';
	let sortOrder: 'asc' | 'desc' = 'desc';

	// Available filter options
	let availableDevices: string[] = [];
	let availableFamilies: string[] = ['deepseek', 'mistral', 'qwen', 'phi', 'whisper'];
	let availableAccelerations: string[] = [];

	// Pagination
	let currentPage = 1;
	let itemsPerPage = 12;

	// Fetch all models from API
	async function fetchAllModels() {
		loading = true;
		error = '';

		try {
			allModels = await foundryModelService.fetchGroupedModels(
				{},
				{ sortBy: 'lastModified', sortOrder: 'desc' }
			);
			updateFilterOptions();
		} catch (err: any) {
			console.error('Failed to fetch models:', err);
			error = 'Failed to fetch models. Please try again later.';
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

		const accelerations = new Set<string>();
		allModels.forEach((model) => {
			if (model.acceleration) {
				accelerations.add(model.acceleration);
			}
			if (model.variants) {
				model.variants.forEach((variant) => {
					if (variant.acceleration) {
						accelerations.add(variant.acceleration);
					}
				});
			}
		});

		availableAccelerations = [...accelerations].sort((a, b) =>
			foundryModelService
				.getAccelerationDisplayName(a)
				.localeCompare(foundryModelService.getAccelerationDisplayName(b))
		);
	}

	// Check if model matches search term
	function matchesSearchTerm(model: GroupedFoundryModel, searchLower: string): boolean {
		if (!searchLower) return true;
		
		return Boolean(
			model.displayName.toLowerCase().includes(searchLower) ||
			model.alias.toLowerCase().includes(searchLower) ||
			model.description.toLowerCase().includes(searchLower) ||
			model.tags.some((tag) => tag.toLowerCase().includes(searchLower)) ||
			model.variants?.some((v) => v.name.toLowerCase().includes(searchLower)) ||
			(model.acceleration && foundryModelService.getAccelerationDisplayName(model.acceleration).toLowerCase().includes(searchLower))
		);
	}

	// Get sort value for a model
	function getSortValue(model: GroupedFoundryModel, sortKey: string): string | number | Date {
		switch (sortKey) {
			case 'displayName':
			case 'name':
				return model.displayName;
			case 'totalDownloads':
			case 'downloadCount':
				return model.totalDownloads || 0;
			case 'fileSizeBytes':
				return model.fileSizeBytes || 0;
			case 'lastModified':
				return model.lastModified;
			default:
				return String((model as unknown as Record<string, unknown>)[sortKey] ?? '');
		}
	}

	function applyFilters() {
		const searchLower = debouncedSearchTerm.toLowerCase();
		
		filteredModels = allModels.filter((model) => {
			const matchesSearch = matchesSearchTerm(model, searchLower);
			const matchesDevice = selectedDevices.length === 0 || selectedDevices.some((device) => model.deviceSupport.includes(device));
			const matchesFamily = !selectedFamily || model.displayName.toLowerCase().includes(selectedFamily.toLowerCase()) || model.alias.toLowerCase().includes(selectedFamily.toLowerCase());
			const matchesAcceleration = !selectedAcceleration || model.acceleration === selectedAcceleration || model.variants?.some((v) => v.acceleration === selectedAcceleration);

			return matchesSearch && matchesDevice && matchesFamily && matchesAcceleration;
		});

		// Apply sorting
		filteredModels.sort((a, b) => {
			let aVal: string | number | Date = getSortValue(a, sortBy);
			let bVal: string | number | Date = getSortValue(b, sortBy);

			if (sortBy === 'lastModified') {
				aVal = new Date(aVal as string);
				bVal = new Date(bVal as string);
			} else if (typeof aVal === 'string') {
				aVal = aVal.toLowerCase();
				bVal = (bVal as string).toLowerCase();
			}

			return sortOrder === 'asc' ? (aVal > bVal ? 1 : -1) : aVal < bVal ? 1 : -1;
		});

		currentPage = 1;
	}

	function clearFilters() {
		searchTerm = '';
		debouncedSearchTerm = '';
		selectedDevices = [];
		selectedFamily = '';
		selectedAcceleration = '';
		sortBy = 'lastModified';
		sortOrder = 'desc';
		currentPage = 1;
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
			const command = `foundry model run ${modelId}`;
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

	// Reactive statements
	$: {
		if (searchDebounceTimer) {
			clearTimeout(searchDebounceTimer);
		}
		searchDebounceTimer = setTimeout(() => {
			debouncedSearchTerm = searchTerm;
		}, 300);
	}

	// Auto-set default sort order only when sortBy changes
	let previousSortBy = sortBy;
	$: if (sortBy !== previousSortBy) {
		if (sortBy === 'fileSizeBytes' || sortBy === 'lastModified' || sortBy === 'downloadCount') {
			sortOrder = 'desc';
		} else {
			sortOrder = 'asc';
		}
		previousSortBy = sortBy;
	}

	$: {
		// Trigger filtering whenever any filter value changes
		selectedDevices;
		selectedFamily;
		selectedAcceleration;
		debouncedSearchTerm;
		sortBy;
		sortOrder;

		if (allModels.length > 0) {
			applyFilters();
		} else {
			filteredModels = [];
		}
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

<Tooltip.Provider delayDuration={150}>
	<Nav />

	<div class="bg-white dark:bg-neutral-950">
		<main id="main-content" class="mx-auto w-full max-w-6xl px-6 py-8 sm:px-8 lg:px-12">
			<ModelFilters
				bind:searchTerm
				bind:selectedDevices
				bind:selectedFamily
				bind:selectedAcceleration
				bind:sortBy
				bind:sortOrder
				{availableDevices}
				{availableFamilies}
				{availableAccelerations}
				filteredCount={filteredModels.length}
				{loading}
				isFiltering={searchTerm !== debouncedSearchTerm}
				onRefresh={refreshModels}
				onClearFilters={clearFilters}
			/>

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
									<path stroke-linecap="round" stroke-linejoin="round" d="M6 18L18 6M6 6l12 12" />
								</svg>
							</div>
							<div class="flex-1">
								<h3 class="font-semibold text-red-900 dark:text-red-100">Error Loading Models</h3>
								<p class="mt-1 text-sm text-red-700 dark:text-red-300">{error}</p>
							</div>
						</div>
					</Card.Content>
				</Card.Root>
			{/if}

			<!-- Models Grid -->
			{#if !loading && !error}
				<ModelGrid
					models={filteredModels}
					bind:currentPage
					{itemsPerPage}
					{copiedModelId}
					onCardClick={openModelDetails}
					onCopyCommand={copyRunCommand}
					onClearFilters={clearFilters}
				/>
			{/if}
		</main>
	</div>

	<!-- Model Details Modal -->
	<ModelDetailsModal
		model={selectedModel}
		bind:isOpen={isModalOpen}
		{copiedModelId}
		onCopyModelId={copyModelId}
		onCopyCommand={copyRunCommand}
	/>

	<Footer />
</Tooltip.Provider>
