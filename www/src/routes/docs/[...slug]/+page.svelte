<script lang="ts">
	import DocRenderer from '$lib/components/document/doc-renderer.svelte';
	import type { PageData } from './$types.js';

	let { data }: { data: PageData } = $props();
	let title = $derived(data.metadata.title);
	let description = $derived(data.metadata.description);
</script>

<DocRenderer {title} {description} {data} />
