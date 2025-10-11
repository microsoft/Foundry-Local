<!-- DocHero.svelte -->
<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import {
		Download,
		DollarSign,
		Shield,
		Box,
		ArrowRight
	} from 'lucide-svelte';
	import { siteConfig } from '$lib/config';

	let { isDownloadOpen = $bindable(false) } = $props();

	function openDownloadDropdown() {
		isDownloadOpen = true;
	}
</script>

<div class="relative overflow-hidden">
	<div class="relative mx-auto max-w-[85rem] px-4 pb-10 pt-24 sm:px-6 lg:px-8">

		<!-- Title -->
		<div class="mx-auto mt-5 max-w-2xl text-center">
			<h1
				class="block text-4xl font-bold text-gray-800 dark:text-neutral-200 md:text-5xl lg:text-6xl"
			>
				<span class="text-primary">Foundry Local</span>
			</h1>
		</div>

		<!-- Description -->
		<div class="mx-auto mt-5 max-w-3xl text-center">
			<p class="text-lg text-gray-600 dark:text-neutral-400">
				{siteConfig.description}
			</p>
		</div>

		<!-- Feature highlights -->
		<div class="mx-auto mt-6 flex max-w-2xl flex-wrap justify-center gap-3">
			<div
				class="inline-flex items-center rounded-full bg-primary/10 px-3 py-1 text-sm text-primary"
			>
				<Box class="mr-1 size-4" /> Run Models Locally
			</div>
			<div
				class="inline-flex items-center rounded-full bg-primary/10 px-3 py-1 text-sm text-primary"
			>
				<DollarSign class="mr-1 size-4" /> Completely Free
			</div>
			<div
				class="inline-flex items-center rounded-full bg-primary/10 px-3 py-1 text-sm text-primary"
			>
				<Shield class="mr-1 size-4" /> Complete Data Privacy
			</div>
		</div>

		<!-- Action Buttons -->
		<div class="mt-8 flex flex-col justify-center gap-3 sm:flex-row">
			<Button 
				variant="default" 
				href="https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started"
				target="_blank"
				rel="noopener noreferrer"
				size="lg"
			>
				Get Started
				<ArrowRight class="ml-2 size-4" />
			</Button>

			<Button variant="outline" onclick={openDownloadDropdown} size="lg" class="border-2">
				<Download class="mr-2 size-4" />
				Download
			</Button>
		</div>
	</div>
</div>
