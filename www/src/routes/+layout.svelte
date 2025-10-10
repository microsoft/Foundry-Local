<script lang="ts">
	import '../app.css';
	let { children } = $props();
	import { ModeWatcher } from 'mode-watcher';
	import { Toaster } from '$lib/components/ui/sonner';
</script>

<Toaster />
<ModeWatcher />
<div class="h-dvh">
	{@render children()}
</div>
