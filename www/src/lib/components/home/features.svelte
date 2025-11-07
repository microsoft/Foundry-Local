<script lang="ts">
	import { features } from '$lib/config';
	import { Button } from '$lib/components/ui/button';
	import { ArrowRight } from 'lucide-svelte';
	import { animate } from '$lib/utils/animations';
	import { onMount } from 'svelte';

	let featureGrid: HTMLElement;

	onMount(() => {
		// Stagger animation for feature cards
		if (featureGrid) {
			const cards = Array.from(featureGrid.children) as HTMLElement[];
			cards.forEach((card, index) => {
				card.style.opacity = '0';
				card.style.transform = 'translateY(30px)';
			});

			const animateCards = () => {
				cards.forEach((card, index) => {
					card.style.transition = 'all 600ms cubic-bezier(0.4, 0, 0.2, 1)';
					card.style.transitionDelay = `${index * 100}ms`;

					requestAnimationFrame(() => {
						card.style.opacity = '1';
						card.style.transform = 'translateY(0)';
					});
				});
			};

			// Check if element is already in viewport
			const rect = featureGrid.getBoundingClientRect();
			const isInViewport = rect.top < window.innerHeight && rect.bottom > 0;

			if (isInViewport) {
				// Trigger animation immediately if already visible
				animateCards();
			} else {
				// Otherwise, wait for it to scroll into view
				const observer = new IntersectionObserver(
					(entries) => {
						entries.forEach((entry) => {
							if (entry.isIntersecting) {
								animateCards();
								observer.unobserve(featureGrid);
							}
						});
					},
					{ threshold: 0.1 }
				);

				observer.observe(featureGrid);
			}
		}
	});
</script>

<div class="bg-white dark:bg-neutral-950">
	<div class="mx-auto max-w-[85rem] px-4 py-16 sm:px-6 lg:px-8 lg:py-20">
		<!-- Section Header -->
		<div class="mx-auto mb-16 max-w-3xl text-center">
			<h2
				use:animate={{ delay: 0, duration: 800, animation: 'slide-up', threshold: 0.2 }}
				class="mb-4 text-3xl font-bold tracking-tight text-gray-900 dark:text-white sm:text-4xl"
			>
				Run Microsoft AI locally with complete control
			</h2>
			<p
				use:animate={{ delay: 200, duration: 800, animation: 'slide-up', threshold: 0.2 }}
				class="text-lg text-gray-600 dark:text-neutral-400"
			>
				Foundry Local brings the power of Azure AI to your environment, with flexible deployment
				options and enterprise-grade security.
			</p>
		</div>

		<!-- Features Grid -->
		<div bind:this={featureGrid} class="grid gap-8 sm:grid-cols-2 lg:grid-cols-4">
			{#each features as feature}
				<div
					class="group relative overflow-hidden rounded-lg border border-gray-200 bg-white shadow-sm transition-all duration-200 ease-out hover:-translate-y-1 hover:shadow-lg dark:border-neutral-800 dark:bg-neutral-900/50"
					style="will-change: transform, box-shadow;"
				>
					<div class="p-6">
						<div
							class="mb-4 flex size-12 items-center justify-center rounded-full bg-primary/10 transition-all duration-200 ease-out group-hover:scale-110 group-hover:bg-primary/20 dark:bg-primary/20"
							style="will-change: transform, background-color;"
						>
							<feature.icon
								class="size-6 shrink-0 text-primary transition-transform duration-200 ease-out group-hover:rotate-6"
								style="will-change: transform;"
							/>
						</div>
						<div>
							<h3 class="mb-2 text-xl font-semibold text-gray-900 dark:text-white">
								{feature.title}
							</h3>
							<p class="text-gray-600 dark:text-neutral-400">
								{feature.description}
							</p>
						</div>
					</div>
				</div>
			{/each}
		</div>
	</div>
</div>
