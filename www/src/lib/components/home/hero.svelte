<!-- DocHero.svelte -->
<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import { Download, DollarSign, Shield, Box, ArrowRight, Rocket } from 'lucide-svelte';
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

		// Stagger animation for buttons - handle nested structure
		if (buttonsContainer) {
			const allButtons: HTMLElement[] = [];
			// Get the first button directly
			const firstButton = buttonsContainer.querySelector('a[href*="get-started"]') as HTMLElement;
			if (firstButton) allButtons.push(firstButton);

			// Get buttons from the secondary actions div
			const secondaryDiv = buttonsContainer.querySelector('div');
			if (secondaryDiv) {
				const secondaryButtons = Array.from(
					secondaryDiv.querySelectorAll('button, a')
				) as HTMLElement[];
				allButtons.push(...secondaryButtons);
			}

			allButtons.forEach((button, index) => {
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

<div
	class="relative overflow-hidden bg-[url('/angle-bracket-triplet.svg')] bg-repeat dark:bg-[url('/angle-bracket-triplet-dark.svg')]"
	style="background-size: 80px 80px;"
>
	<div class="relative mx-auto max-w-[85rem] px-4 pb-10 pt-24 sm:px-6 lg:px-8">
		<!-- Brand mark -->
		<div
			class="mx-auto flex max-w-2xl justify-center"
			use:animate={{ delay: 0, duration: 700, animation: 'fade-in' }}
		>
			<img src={siteConfig.logo} alt="Foundry Local logo" class="h-16 dark:hidden" />
			{#if siteConfig.logoDark}
				<img src={siteConfig.logoDark} alt="Foundry Local logo" class="hidden h-16 dark:block" />
			{:else}
				<img src={siteConfig.logo} alt="Foundry Local logo" class="hidden h-16 dark:block" />
			{/if}
		</div>

		<!-- Title -->
		<div class="mx-auto mt-5 max-w-2xl text-center">
			<h1
				use:animate={{ delay: 150, duration: 800, animation: 'slide-up' }}
				class="block text-4xl font-bold text-gray-800 md:text-5xl lg:text-6xl dark:text-neutral-200"
			>
				<span class="text-primary">Foundry Local</span>
			</h1>
		</div>

		<!-- Description -->
		<div class="mx-auto mt-5 max-w-3xl text-center">
			<p
				use:animate={{ delay: 350, duration: 800, animation: 'slide-up' }}
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
				class="bg-primary/10 text-primary hover:bg-primary/20 inline-flex items-center rounded-full px-3 py-1 text-sm transition-all duration-300 hover:scale-105"
			>
				<Box class="mr-1 size-4" /> Run Models Locally
			</div>
			<div
				class="bg-primary/10 text-primary hover:bg-primary/20 inline-flex items-center rounded-full px-3 py-1 text-sm transition-all duration-300 hover:scale-105"
			>
				<DollarSign class="mr-1 size-4" /> Free to Use
			</div>
			<div
				class="bg-primary/10 text-primary hover:bg-primary/20 inline-flex items-center rounded-full px-3 py-1 text-sm transition-all duration-300 hover:scale-105"
			>
				<Shield class="mr-1 size-4" /> Data Privacy
			</div>
		</div>

		<!-- Action Buttons -->
		<div bind:this={buttonsContainer} class="mt-8 flex flex-col items-center justify-center gap-3">
			<!-- Primary CTA -->
			<Button
				variant="default"
				href="https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started"
				target="_blank"
				rel="noopener noreferrer"
				size="lg"
				class="group w-full transition-all duration-300 hover:scale-105 hover:shadow-lg sm:w-40"
			>
				<Rocket class="mr-2 size-4 transition-transform duration-300 group-hover:-translate-y-1" />
				Get Started
			</Button>

			<!-- Secondary Actions -->
			<div class="flex w-full flex-col items-center gap-3 sm:flex-row sm:justify-center">
				<Button
					variant="outline"
					href="/models"
					size="lg"
					class="group w-full border-2 transition-all duration-300 hover:scale-105 hover:shadow-lg sm:w-40"
				>
					<Box class="mr-2 size-4 transition-transform duration-300 group-hover:rotate-12" />
					Models
				</Button>
				<Button
					variant="outline"
					onclick={openDownloadDropdown}
					size="lg"
					class="group w-full border-2 transition-all duration-300 hover:scale-105 hover:shadow-lg sm:w-40"
				>
					<Download
						class="mr-2 size-4 transition-transform duration-300 group-hover:translate-y-0.5"
					/>
					Download
				</Button>
			</div>
		</div>
	</div>
</div>
