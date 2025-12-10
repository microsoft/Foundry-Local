<script lang="ts">
	import { Moon, Sun } from 'lucide-svelte';
	import { toggleMode, mode } from 'mode-watcher';
	import { Button } from '$lib/components/ui/button/';

	let currentMode = $derived(mode.current);
</script>

<Button
	onclick={toggleMode}
	variant="ghost"
	size="icon"
	aria-label={currentMode === 'dark' ? 'Switch to light mode' : 'Switch to dark mode'}
>
	<Sun
		class="h-[1.2rem] w-[1.2rem] rotate-0 scale-100 transition-all dark:-rotate-90 dark:scale-0"
		aria-hidden="true"
	/>
	<Moon
		class="absolute h-[1.2rem] w-[1.2rem] rotate-90 scale-0 transition-all dark:rotate-0 dark:scale-100"
		aria-hidden="true"
	/>

	<span class="sr-only">Toggle theme</span>
</Button>
