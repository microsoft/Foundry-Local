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
		Check
	} from 'lucide-svelte';
	import * as Card from '$lib/components/ui/card';
	import { toast } from 'svelte-sonner';

	// Debounce utility for search
	let searchDebounceTimer: ReturnType<typeof setTimeout> | null = null;
	let debouncedSearchTerm = '';

	// State
	let allModels: GroupedFoundryModel[] = [];
	let filteredModels: GroupedFoundryModel[] = [];
	let loading = false;
	let error = '';
	let copiedModelId: string | null = null;

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
		availableAccelerations = [
			...new Set(allModels.map((m) => m.acceleration).filter((h): h is string => !!h))
		].sort();
	}

	function applyFilters() {
		filteredModels = allModels.filter((model) => {
			const matchesSearch =
				!debouncedSearchTerm ||
				model.displayName.toLowerCase().includes(debouncedSearchTerm.toLowerCase()) ||
				model.alias.toLowerCase().includes(debouncedSearchTerm.toLowerCase()) ||
				model.description.toLowerCase().includes(debouncedSearchTerm.toLowerCase()) ||
				model.tags.some((tag) => tag.toLowerCase().includes(debouncedSearchTerm.toLowerCase()));

			const matchesDevice =
				selectedDevices.length === 0 ||
				selectedDevices.some((device) => model.deviceSupport.includes(device));
			const matchesFamily =
				!selectedFamily ||
				model.displayName.toLowerCase().includes(selectedFamily.toLowerCase()) ||
				model.alias.toLowerCase().includes(selectedFamily.toLowerCase());
			const matchesAcceleration =
				!selectedAcceleration || model.acceleration === selectedAcceleration;

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
				<h1 class="mb-3 text-3xl font-bold text-gray-800 dark:text-neutral-200 md:text-4xl">
					Foundry <span class="text-primary">Local Models</span>
				</h1>
				<p class="text-base text-gray-600 dark:text-neutral-400">
					Explore AI models optimized for local deployment. Search and filter to find models for
					your NPUs, GPUs, and CPUs.
				</p>
			</div>
		</div>
	</div>

	<!-- Main Content -->
	<main id="main-content" class="mx-auto max-w-[85rem] px-4 py-6 sm:px-6 lg:px-8">
		<!-- Filters Section -->
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
								<option value="downloadCount">Downloads</option>
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
				<div class="grid gap-6 sm:grid-cols-2 lg:grid-cols-3">
					{#each paginatedModels as model (model.alias)}
						<Card.Root class="flex flex-col transition-all hover:shadow-lg">
							<Card.Header>
								<Card.Title class="line-clamp-1">{model.displayName}</Card.Title>
								<Card.Description class="text-xs">{model.publisher}</Card.Description>
							</Card.Header>
							<Card.Content class="flex-1">
								<p class="mb-4 line-clamp-3 text-sm text-gray-600 dark:text-gray-400">
									{model.description}
								</p>

								<!-- Badges with Date and Version -->
								<div class="mb-4 flex flex-wrap items-center justify-between gap-2">
									<div class="flex items-center gap-1 text-xs text-gray-500">
										<Calendar class="size-3" />
										<span>{formatDate(model.lastModified)}</span>
									</div>
									<span class="text-xs text-gray-400">•</span>
									<Badge variant="secondary" class="text-xs">v{model.latestVersion}</Badge>
									{#if model.taskType}
										<span class="text-xs text-gray-400">•</span>
										<Badge variant="secondary" class="text-xs">{model.taskType}</Badge>
									{/if}
									{#if model.license}
										<span class="text-xs text-gray-400">•</span>
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

								<!-- Copy Model ID Section -->
								<div class="border-t pt-4">
									{#if model.variants && model.variants.length > 0}
										{@const uniqueVariants = getUniqueVariants(model)}
										{@const sortedVariants = sortVariantsByDevice(uniqueVariants)}
										<div class="flex flex-wrap items-center gap-2">
											<span class="text-xs font-medium text-gray-600 dark:text-gray-400"
												>Copy Model ID:</span
											>
											{#each sortedVariants as variant}
												{@const primaryDevice = variant.deviceSupport[0]?.toUpperCase() || ''}
												<Button
													variant="outline"
													size="sm"
													onclick={() => copyModelId(variant.name)}
													class="h-7 gap-1 px-2 text-xs"
												>
													{#if copiedModelId === variant.name}
														<Check class="size-3 text-green-500" />
														<span>Copied!</span>
													{:else}
														{#each variant.deviceSupport as device}
															<span>{getDeviceIcon(device)}</span>
														{/each}
														<span>{primaryDevice}</span>
													{/if}
												</Button>
											{/each}
										</div>
									{/if}
								</div>
							</Card.Content>
						</Card.Root>
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

<Footer />

<style>
	.line-clamp-1 {
		display: -webkit-box;
		-webkit-line-clamp: 1;
		line-clamp: 1;
		-webkit-box-orient: vertical;
		overflow: hidden;
	}

	.line-clamp-3 {
		display: -webkit-box;
		-webkit-line-clamp: 3;
		line-clamp: 3;
		-webkit-box-orient: vertical;
		overflow: hidden;
	}
</style>
