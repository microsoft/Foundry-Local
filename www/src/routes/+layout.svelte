<script lang="ts">
	import '../app.css';
	import { ModeWatcher } from 'mode-watcher';
	import { Toaster } from '$lib/components/ui/sonner';
	import { onNavigate } from '$app/navigation';
	import { inject } from '@vercel/analytics';
	import { AlertTriangle } from 'lucide-svelte';

	let { children } = $props();

	// Inject Vercel Analytics
	inject();

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
	<div
		role="status"
		aria-live="polite"
		class="border-b border-warning/30 bg-warning/15 px-4 py-2.5 text-sm text-foreground shadow-sm sm:px-6 lg:px-8"
	>
		<div class="mx-auto flex w-full max-w-[85rem] items-start justify-center gap-2.5 sm:items-center">
			<AlertTriangle class="mt-0.5 size-4 shrink-0 text-warning sm:mt-0" aria-hidden="true" />
			<p class="max-w-4xl font-medium leading-6">
				We’re currently experiencing intermittent model download issues due to high demand and are
				actively working on a fix.
			</p>
		</div>
	</div>
	{@render children()}
</div>
