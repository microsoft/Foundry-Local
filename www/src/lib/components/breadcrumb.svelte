<script lang="ts">
	import { page } from '$app/state';
	import * as Breadcrumb from '$lib/components/ui/breadcrumb/index.js';
	const path = $derived(page.url.pathname);
	const segments = $derived(path.split('/').filter(Boolean)); // split path and remove empty segments
	const breadcrumbs = $derived(
		segments.map((segment, index) => {
			// Construct the path up to this segment
			const href = '/' + segments.slice(0, index + 1).join('/');
			return {
				name: segment.replace(/-/g, ' ').replace(/\b\w/g, (char) => char.toUpperCase()),
				href
			};
		})
	);
</script>

<Breadcrumb.Root>
	<Breadcrumb.List>
		{#each breadcrumbs as breadcrumb, i}
			{#if i > 0}
				<Breadcrumb.Separator class="hidden md:block" />
			{/if}
			<Breadcrumb.Item>
				<!-- Check if it's the last breadcrumb item to render as Page instead of Link -->
				{#if i === breadcrumbs.length - 1}
					<Breadcrumb.Page class="font-semibold text-foreground" aria-current="page">
						{breadcrumb.name}
					</Breadcrumb.Page>
				{:else}
					<Breadcrumb.Link href={breadcrumb.href} class="hidden md:block"
						>{breadcrumb.name}</Breadcrumb.Link
					>
				{/if}
			</Breadcrumb.Item>
		{/each}
	</Breadcrumb.List>
</Breadcrumb.Root>
