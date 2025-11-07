<script lang="ts">
	import type { GroupedFoundryModel } from '../types';
	import { Button } from '$lib/components/ui/button';
	import * as Card from '$lib/components/ui/card';
	import ModelCard from './ModelCard.svelte';

	export let models: GroupedFoundryModel[] = [];
	export let currentPage = 1;
	export let itemsPerPage = 12;
	export let copiedModelId: string | null = null;
	export let onCardClick: (model: GroupedFoundryModel) => void;
	export let onCopyCommand: (modelId: string) => void;
	export let onClearFilters: () => void;

	$: totalPages = Math.ceil(models.length / itemsPerPage);
	$: paginatedModels = models.slice((currentPage - 1) * itemsPerPage, currentPage * itemsPerPage);

	function goToPage(page: number) {
		currentPage = Math.max(1, Math.min(totalPages, page));
	}
</script>

{#if models.length > 0}
	<div class="mt-6 grid auto-rows-fr gap-4 sm:grid-cols-2 lg:grid-cols-3">
		{#each paginatedModels as model (model.id)}
			<ModelCard {model} {copiedModelId} {onCardClick} {onCopyCommand} />
		{/each}
	</div>

	<!-- Pagination -->
	{#if totalPages > 1}
		<div class="mt-8 flex justify-center gap-2">
			<Button
				variant="outline"
				size="sm"
				disabled={currentPage === 1}
				onclick={() => goToPage(currentPage - 1)}
			>
				Previous
			</Button>

			<div class="flex items-center gap-2">
				{#each Array(totalPages).fill(0) as _, i}
					{#if i + 1 === 1 || i + 1 === totalPages || (i + 1 >= currentPage - 2 && i + 1 <= currentPage + 2)}
						<Button
							variant={currentPage === i + 1 ? 'default' : 'outline'}
							size="sm"
							onclick={() => goToPage(i + 1)}
						>
							{i + 1}
						</Button>
					{:else if i + 1 === currentPage - 3 || i + 1 === currentPage + 3}
						<span class="px-2">...</span>
					{/if}
				{/each}
			</div>

			<Button
				variant="outline"
				size="sm"
				disabled={currentPage === totalPages}
				onclick={() => goToPage(currentPage + 1)}
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
			<Button onclick={onClearFilters}>Clear All Filters</Button>
		</Card.Content>
	</Card.Root>
{/if}
