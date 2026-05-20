<!-- DocHero.svelte -->
<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { BookOpen, FileText, Box } from 'lucide-svelte';
	import { siteConfig } from '$lib/config';
	import { animate } from '$lib/utils/animations';
	import LogoTransition from '$lib/components/logo-transition.svelte';
	import InstallCommand from '$lib/components/install-command.svelte';
	import { onMount } from 'svelte';

	let buttonsContainer: HTMLElement;
	let installCommandContainer: HTMLElement;

	onMount(() => {
		// Animation for install command
		if (installCommandContainer) {
			installCommandContainer.style.opacity = '0';
			installCommandContainer.style.transform = 'translateY(20px)';
			installCommandContainer.style.transition = 'all 600ms cubic-bezier(0.4, 0, 0.2, 1)';
			installCommandContainer.style.transitionDelay = '900ms';

			requestAnimationFrame(() => {
				installCommandContainer.style.opacity = '1';
				installCommandContainer.style.transform = 'translateY(0)';
			});
		}

		// Stagger animation for buttons - handle nested structure
		if (buttonsContainer) {
			const allButtons: HTMLElement[] = [];
			const buttons = Array.from(buttonsContainer.querySelectorAll('button, a')) as HTMLElement[];
			allButtons.push(...buttons);

			allButtons.forEach((button, index) => {
				button.style.opacity = '0';
				button.style.transform = 'translateY(20px)';
				button.style.transition = 'all 600ms cubic-bezier(0.4, 0, 0.2, 1)';
				button.style.transitionDelay = `${1100 + index * 100}ms`;

				requestAnimationFrame(() => {
					button.style.opacity = '1';
					button.style.transform = 'translateY(0)';
				});
			});
		}
	});
</script>

<div class="relative overflow-hidden">
	<div class="relative mx-auto max-w-[85rem] px-4 pt-24 pb-10 sm:px-6 lg:px-8">
		<!-- Brand mark -->
		<div
			class="mx-auto flex max-w-2xl justify-center"
			use:animate={{ delay: 0, duration: 700, animation: 'fade-in' }}
		>
			<span class="logo-hover-target inline-flex">
				<LogoTransition
					colorSrc={siteConfig.logo}
					darkSrc={siteConfig.logoDark ?? siteConfig.logo}
					strokeSrc={siteConfig.logoMark}
					height={64}
					alt="Foundry Local logo"
				/>
			</span>
		</div>

		<!-- Title -->
		<div class="mx-auto mt-5 max-w-2xl text-center">
			<h1
				use:animate={{ delay: 150, duration: 800, animation: 'slide-up' }}
				class="block text-4xl font-bold text-gray-800 md:text-5xl lg:text-6xl dark:text-neutral-200"
			>
				<span class="text-primary">Build local AI</span><br />
				into your app
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

		<!-- Action Buttons -->
		<div class="mt-8 flex flex-col items-center justify-center gap-6">
			<!-- Install Command - Primary CTA -->
			<div bind:this={installCommandContainer} class="w-full max-w-4xl px-4 sm:px-6">
				<InstallCommand />
			</div>

			<!-- Secondary Actions -->
			<div
				bind:this={buttonsContainer}
				class="flex w-full max-w-3xl flex-col items-stretch gap-3 px-4 sm:flex-row sm:justify-center sm:px-6"
			>
				<Button
					variant="outline"
					href="https://learn.microsoft.com/en-us/azure/foundry-local/get-started"
					target="_blank"
					rel="noopener noreferrer"
					size="lg"
					class="group min-h-[44px] flex-1 border-2 transition-all duration-300 hover:scale-105 hover:shadow-lg"
					aria-label="Read the SDK documentation (opens in new tab)"
				>
					<BookOpen
						class="mr-2 size-4 transition-transform duration-300 group-hover:scale-110"
						aria-hidden="true"
					/>
					SDK Docs
				</Button>
				<Button
					variant="outline"
					href="https://github.com/microsoft/Foundry-Local/tree/main/samples"
					target="_blank"
					rel="noopener noreferrer"
					size="lg"
					class="group min-h-[44px] flex-1 border-2 transition-all duration-300 hover:scale-105 hover:shadow-lg"
					aria-label="Browse SDK samples on GitHub (opens in new tab)"
				>
					<FileText
						class="mr-2 size-4 transition-transform duration-300 group-hover:scale-110"
						aria-hidden="true"
					/>
					Samples
				</Button>
				<Button
					variant="outline"
					href="/models"
					size="lg"
					class="group min-h-[44px] flex-1 border-2 transition-all duration-300 hover:scale-105 hover:shadow-lg"
					aria-label="Browse available AI models"
				>
					<Box
						class="mr-2 size-4 transition-transform duration-300 group-hover:rotate-12"
						aria-hidden="true"
					/>
					Models
				</Button>
			</div>
		</div>
	</div>
</div>
