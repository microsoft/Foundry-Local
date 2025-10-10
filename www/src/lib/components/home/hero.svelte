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
		Shield
	} from 'lucide-svelte';
	import { siteConfig } from '$lib/config';
	import { toast } from 'svelte-sonner';

	let isCopied = false;
	const installCommand = `winget install Microsoft.FoundryLocal`;

	async function copyInstallCommand() {
		try {
			await navigator.clipboard.writeText(installCommand);
			isCopied = true;
			toast.success('Copied to clipboard');
			// Reset the copied state after 2 seconds
			setTimeout(() => {
				isCopied = false;
			}, 2000);
		} catch (err) {
			toast.error('Failed to copy to clipboard');
		}
	}
</script>

<div class="relative overflow-hidden">
	<div class="relative mx-auto max-w-[85rem] px-4 pb-10 pt-24 sm:px-6 lg:px-8">
		<!-- Version badge -->
		<div class="flex justify-center">
			<a
				href={siteConfig.github}
				target="_blank"
				rel="noopener noreferrer"
				class="inline-flex items-center gap-x-2 rounded-full border border-gray-200 bg-white p-1 ps-3 text-sm text-gray-800 transition hover:border-gray-300 dark:border-neutral-700 dark:bg-neutral-800 dark:text-neutral-200"
			>
				Version {siteConfig.version} - Microsoft Official Release
				<Badge variant="secondary" class="gap-x-2 rounded-full">
					<span class="hidden sm:inline">View on GitHub</span>
					<ChevronRight class="size-4" />
				</Badge>
			</a>
		</div>

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
				<Server class="mr-1 size-4" /> Run Models Locally
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

			<Button variant="outline" class="font-mono" onclick={copyInstallCommand} size="lg">
				{installCommand}
				<span class="ml-2 rounded bg-gray-200 p-1 dark:bg-neutral-700">
					{#if isCopied}
						<Check class="size-4 text-green-500 transition-transform" />
					{:else}
						<Terminal class="size-4 transition-transform hover:rotate-6" />
					{/if}
				</span>
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
