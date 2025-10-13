<script lang="ts">
	import '../app.css';
	let { children } = $props();
	import { ModeWatcher } from 'mode-watcher';
	import { Toaster } from '$lib/components/ui/sonner';
	import { onNavigate } from '$app/navigation';
	import { injectAnalytics } from '@vercel/analytics/sveltekit';

	// Inject Vercel Analytics
	injectAnalytics();

	// Page transition animation
	onNavigate((navigation) => {
		if (!document.startViewTransition) return;

		return new Promise((resolve) => {
			document.startViewTransition(async () => {
				resolve();
				await navigation.complete;
			});
		});
	});
</script>

<Toaster />
<ModeWatcher />
<div class="h-dvh">
	{@render children()}
</div>
