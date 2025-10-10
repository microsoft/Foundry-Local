<script>
	import { page } from '$app/state';

	let { title, description } = $props();
</script>

<header class="mb-4 border-b border-gray-200 pb-2 dark:border-neutral-700 sm:mb-4 sm:pb-6">
	{#if page.params.slug}
		<p class="mb-2 text-sm font-semibold capitalize text-primary">
			{page.params.slug
				.replace(/-/g, ' ')
				.replace(/\b\w/g, (char) => char.toUpperCase())
				.replace('/', ': ')}
		</p>
	{:else}
		<p class="mb-2 text-sm font-semibold capitalize text-primary">Docs</p>
	{/if}
	<h1 class="mb-2 block text-2xl font-bold sm:text-3xl">
		{title}
	</h1>
	<p class="mt-2 text-gray-800 dark:text-neutral-400 sm:text-lg">
		{description}
	</p>
</header>
