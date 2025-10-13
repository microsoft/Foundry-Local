<!-- DocHero.svelte -->
<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import { Download, DollarSign, Shield, Box, ArrowRight } from 'lucide-svelte';
	import { siteConfig } from '$lib/config';
	import { animate } from '$lib/utils/animations';
	import { onMount } from 'svelte';

	let { isDownloadOpen = $bindable(false) } = $props();

	function openDownloadDropdown() {
		isDownloadOpen = true;
	}

	let badgesContainer: HTMLElement;
	let buttonsContainer: HTMLElement;

	onMount(() => {
		// Stagger animation for badges
		if (badgesContainer) {
			const badges = Array.from(badgesContainer.children) as HTMLElement[];
			badges.forEach((badge, index) => {
				badge.style.opacity = '0';
				badge.style.transform = 'translateY(20px)';
				badge.style.transition = 'all 600ms cubic-bezier(0.4, 0, 0.2, 1)';
				badge.style.transitionDelay = `${600 + index * 100}ms`;

				requestAnimationFrame(() => {
					badge.style.opacity = '1';
					badge.style.transform = 'translateY(0)';
				});
			});
		}

		// Stagger animation for buttons
		if (buttonsContainer) {
			const buttons = Array.from(buttonsContainer.children) as HTMLElement[];
			buttons.forEach((button, index) => {
				button.style.opacity = '0';
				button.style.transform = 'translateY(20px)';
				button.style.transition = 'all 600ms cubic-bezier(0.4, 0, 0.2, 1)';
				button.style.transitionDelay = `${900 + index * 100}ms`;

				requestAnimationFrame(() => {
					button.style.opacity = '1';
					button.style.transform = 'translateY(0)';
				});
			});
		}
	});
</script>

<div class="relative overflow-hidden">
	<div class="relative mx-auto max-w-[85rem] px-4 pb-10 pt-24 sm:px-6 lg:px-8">
		<!-- Title -->
		<div class="mx-auto mt-5 max-w-2xl text-center">
			<h1
				use:animate={{ delay: 0, duration: 800, animation: 'slide-up' }}
				class="block text-4xl font-bold text-gray-800 dark:text-neutral-200 md:text-5xl lg:text-6xl"
			>
				<span class="text-primary">Foundry Local</span>
			</h1>
		</div>

		<!-- Description -->
		<div class="mx-auto mt-5 max-w-3xl text-center">
			<p
				use:animate={{ delay: 200, duration: 800, animation: 'slide-up' }}
				class="text-lg text-gray-600 dark:text-neutral-400"
			>
				{siteConfig.description}
			</p>
		</div>

		<!-- Feature highlights -->
		<div
			bind:this={badgesContainer}
			class="mx-auto mt-6 flex max-w-2xl flex-wrap justify-center gap-3"
		>
			<div
				class="inline-flex items-center rounded-full bg-primary/10 px-3 py-1 text-sm text-primary transition-all duration-300 hover:scale-105 hover:bg-primary/20"
			>
				<Box class="mr-1 size-4" /> Run Models Locally
			</div>
			<div
				class="inline-flex items-center rounded-full bg-primary/10 px-3 py-1 text-sm text-primary transition-all duration-300 hover:scale-105 hover:bg-primary/20"
			>
				<DollarSign class="mr-1 size-4" /> Completely Free
			</div>
			<div
				class="inline-flex items-center rounded-full bg-primary/10 px-3 py-1 text-sm text-primary transition-all duration-300 hover:scale-105 hover:bg-primary/20"
			>
				<Shield class="mr-1 size-4" /> Complete Data Privacy
			</div>
		</div>

		<!-- Action Buttons -->
		<div bind:this={buttonsContainer} class="mt-8 flex flex-col justify-center gap-3 sm:flex-row">
			<Button
				variant="default"
				href="https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started"
				target="_blank"
				rel="noopener noreferrer"
				size="lg"
				class="group transition-all duration-300 hover:scale-105 hover:shadow-lg"
			>
				Get Started
				<ArrowRight
					class="ml-2 size-4 transition-transform duration-300 group-hover:translate-x-1"
				/>
			</Button>

			<Button
				variant="outline"
				onclick={openDownloadDropdown}
				size="lg"
				class="group border-2 transition-all duration-300 hover:scale-105 hover:shadow-lg"
			>
				<Download
					class="mr-2 size-4 transition-transform duration-300 group-hover:translate-y-0.5"
				/>
				Download
			</Button>
		</div>
	</div>
</div>
