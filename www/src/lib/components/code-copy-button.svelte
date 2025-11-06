<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Check, Copy } from 'lucide-svelte';
	import { toast } from 'svelte-sonner';

	let { code }: { code: string } = $props();
	let copied = $state(false);

	async function copyToClipboard() {
		try {
			await navigator.clipboard.writeText(code);
			copied = true;
			toast.success('Copied to clipboard');
			setTimeout(() => {
				copied = false;
			}, 2000);
		} catch (err) {
			toast.error('Failed to copy to clipboard');
		}
	}
</script>

<Button
	variant="ghost"
	size="icon"
	class="absolute right-2 top-2 size-8 opacity-70 hover:opacity-100"
	onclick={copyToClipboard}
	aria-label="Copy code to clipboard"
>
	{#if copied}
		<Check class="size-4 text-success" aria-hidden="true" />
		<span class="sr-only">Copied</span>
	{:else}
		<Copy class="size-4" aria-hidden="true" />
	{/if}
</Button>
