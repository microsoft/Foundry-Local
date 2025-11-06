<script lang="ts">
	import { promoConfig } from '$lib/config';
	import { ArrowRight, Cloud } from 'lucide-svelte';
	import Button from '../ui/button/button.svelte';
</script>

<div
	class="rounded-lg border border-neutral-200 bg-gradient-to-r from-primary/5 to-primary/10 p-6 dark:border-neutral-800 dark:from-primary/10 dark:to-primary/5"
>
	<div class="flex items-start">
		<div
			class="mr-4 flex h-12 w-12 items-center justify-center rounded-full bg-primary/20 text-primary dark:bg-primary/30 dark:text-primary"
		>
			<Cloud size={24} />
		</div>

		<div>
			<h3 class="text-lg font-semibold text-gray-900 dark:text-white">
				{promoConfig.title}
			</h3>
			<p class="mt-2 text-sm text-gray-700 dark:text-neutral-300">
				{promoConfig.description}
			</p>
			<div class="mt-4">
				<Button variant="outline" href={promoConfig.ctaLink} class="inline-flex items-center">
					{promoConfig.ctaText}
					<ArrowRight class="ml-2 h-4 w-4" />
				</Button>
			</div>
		</div>
	</div>
</div>
