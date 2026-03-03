<script lang="ts">
	import type { GroupedFoundryModel } from '../types';
	import { Button } from '$lib/components/ui/button';
	import * as Card from '$lib/components/ui/card';
	import ModelCard from './ModelCard.svelte';

	export let models: GroupedFoundryModel[] = [];
	export let copiedModelId: string | null = null;
	export let onCardClick: (model: GroupedFoundryModel) => void;
	export let onCopyCommand: (modelId: string) => void;
	export let onClearFilters: () => void;
</script>

{#if models.length > 0}
	<div class="mt-6 grid auto-rows-fr gap-4 sm:grid-cols-2 lg:grid-cols-3">
		{#each models as model (model.id)}
			<ModelCard {model} {copiedModelId} {onCardClick} {onCopyCommand} />
		{/each}
	</div>
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
