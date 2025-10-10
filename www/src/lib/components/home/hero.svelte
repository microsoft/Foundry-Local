<!-- DocHero.svelte -->
<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import {
		ChevronRight,
		Download,
		Server,
		Terminal,
		Check,
		DollarSign,
		Shield,
		Box
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
				href="https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started"
				target="_blank"
				rel="noopener noreferrer"
				variant="default"
				size="lg"
			>
				Get Started
				<ChevronRight class="size-4" />
			</Button>

			<Button variant="outline" onclick={openDownloadDropdown} size="lg">
				<Download class="mr-2 size-4" />
				Download
			</Button>
		</div>

		<!-- Quick Links -->
		<div class="mt-5 flex flex-wrap items-center justify-center gap-2">
			<span class="text-sm text-gray-600 dark:text-neutral-400">Quick links:</span>
			{#each siteConfig.quickLinks as link}
				<Button 
					variant="link" 
					href={link.href} 
					class="h-auto p-1"
					target={link.href?.startsWith('http') ? '_blank' : undefined}
					rel={link.href?.startsWith('http') ? 'noopener noreferrer' : undefined}
				>
					{link.title}
					<ChevronRight class="size-4" />
				</Button>
				{#if link !== siteConfig.quickLinks[siteConfig.quickLinks.length - 1]}
					<span class="text-gray-300 dark:text-neutral-600">•</span>
				{/if}
			{/each}

			<Button
				variant="link"
				href="https://azure.microsoft.com/en-us/products/ai-studio"
				target="_blank"
				rel="noopener noreferrer"
				class="h-auto p-1"
			>
				Azure AI Foundry
				<svg class="ml-1 size-4" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
					<path
						d="M7 17L17 7"
						stroke="currentColor"
						stroke-width="1.5"
						stroke-linecap="round"
						stroke-linejoin="round"
					/>
					<path
						d="M7 7H17V17"
						stroke="currentColor"
						stroke-width="1.5"
						stroke-linecap="round"
						stroke-linejoin="round"
					/>
				</svg>
			</Button>
		</div>
	</div>
</div>
