<script lang="ts">
	import { promoConfig } from '$lib/config';
	import { ArrowRight, Cloud } from 'lucide-svelte';
	import Button from '../ui/button/button.svelte';
</script>

<div
	class="rounded-lg border border-neutral-200 bg-gradient-to-r from-blue-50 to-indigo-50 p-6 dark:border-neutral-800 dark:from-blue-950/30 dark:to-indigo-950/30"
>
	<div class="flex items-start">
		<div
			class="mr-4 flex h-12 w-12 items-center justify-center rounded-full bg-blue-100 text-blue-600 dark:bg-blue-900/50 dark:text-blue-400"
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
