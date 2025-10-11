<script lang="ts">
	import DocRenderer from '$lib/components/document/doc-renderer.svelte';
	import type { PageData } from './$types.js';

	let { data }: { data: PageData } = $props();
	let { title, description } = data.metadata;
</script>

<DocRenderer {title} {description} {data} />
