<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Input } from '$lib/components/ui/input';
	import { Label } from '$lib/components/ui/label';
	import { Search, RefreshCw } from 'lucide-svelte';
	import * as Card from '$lib/components/ui/card';
	import { foundryModelService } from '../service';

	export let searchTerm = '';
	export let selectedDevices: string[] = [];
	export let selectedFamily = '';
	export let selectedAcceleration = '';
	export let sortBy = 'name';
	export let sortOrder: 'asc' | 'desc' = 'asc';
	export let availableDevices: string[] = [];
	export let availableFamilies: string[] = [];
	export let availableAccelerations: string[] = [];
	export let filteredCount = 0;
	export let loading = false;
	export let isFiltering = false;

	export let onRefresh: () => void;
	export let onClearFilters: () => void;

	function toggleDevice(device: string) {
		if (selectedDevices.includes(device)) {
			selectedDevices = selectedDevices.filter((d) => d !== device);
		} else {
			selectedDevices = [...selectedDevices, device];
		}
	}

	function getDeviceIcon(device: string): string {
		const icons: Record<string, string> = {
			npu: '🧠',
			gpu: '🎮',
			cpu: '💻'
		};
		return icons[device.toLowerCase()] || '🔧';
	}

	$: hasActiveFilters =
		searchTerm || selectedDevices.length > 0 || selectedFamily || selectedAcceleration;
</script>

<Card.Root class="border-border bg-background border shadow-sm dark:border-neutral-800">
	<Card.Header class="flex flex-col gap-4 sm:flex-row sm:items-start sm:justify-between">
		<div>
			<Card.Title class="text-2xl font-semibold">Browse Foundry Models</Card.Title>
			<Card.Description>Results update automatically as you type or change filters</Card.Description
			>
		</div>
		<Button variant="ghost" size="sm" onclick={onRefresh} disabled={loading}>
			<RefreshCw class={`mr-1 size-4 ${loading ? 'animate-spin' : ''}`} />
			Refresh
		</Button>
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
						class="border-input bg-background flex h-10 w-full rounded-md border px-3 py-2 text-sm"
					>
						<option value="name">Name</option>
						<option value="lastModified">Last Modified</option>
					</select>
					<select
						bind:value={sortOrder}
						class="border-input bg-background flex h-10 rounded-md border px-3 py-2 text-sm"
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
					class="border-input bg-background flex h-10 w-full rounded-md border px-3 py-2 text-sm"
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
					class="border-input bg-background flex h-10 w-full rounded-md border px-3 py-2 text-sm"
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
				{#if isFiltering}
					<span class="inline-flex items-center">
						<div
							class="border-primary mr-2 size-4 animate-spin rounded-full border-2 border-t-transparent"
						></div>
						Filtering...
					</span>
				{:else}
					{filteredCount} model{filteredCount !== 1 ? 's' : ''} found
				{/if}
			</div>
			{#if hasActiveFilters}
				<Button variant="outline" size="sm" onclick={onClearFilters}>Clear Filters</Button>
			{/if}
		</div>
	</Card.Content>
</Card.Root>
