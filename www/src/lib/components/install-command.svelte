<script lang="ts">
	import { Copy, Check, Terminal } from 'lucide-svelte';
	import { toast } from 'svelte-sonner';
	import { onMount } from 'svelte';
	import { getInstallCommand } from '$lib/utils/platform';

	let copied = $state(false);
	let copiedModel = $state(false);
	let showModelCommand = $state(false);
	let commandElement: HTMLElement;
	let modelCommandElement: HTMLElement;
	let installInfo = $state({
		command: 'brew install microsoft/foundrylocal/foundrylocal',
		label: 'Install',
		description: 'Install via Homebrew'
	});

	const modelCommand = 'foundry model run qwen2.5-0.5b';

	// Platform SVG icons
	const AppleIcon = `<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" fill="currentColor" viewBox="0 0 16 16"><path d="M11.182.008C11.148-.03 9.923.023 8.857 1.18c-1.066 1.156-.902 2.482-.878 2.516.024.034 1.52.087 2.475-1.258.955-1.345.762-2.391.728-2.43zm3.314 11.733c-.048-.096-2.325-1.234-2.113-3.422.212-2.189 1.675-2.789 1.698-2.854.023-.065-.597-.79-1.254-1.157a3.692 3.692 0 0 0-1.563-.434c-.108-.003-.483-.095-1.254.116-.508.139-1.653.589-1.968.607-.316.018-1.256-.522-2.267-.665-.647-.125-1.333.131-1.824.328-.49.196-1.422.754-2.074 2.237-.652 1.482-.311 3.83-.067 4.56.244.729.625 1.924 1.273 2.796.576.984 1.34 1.667 1.659 1.899.319.232 1.219.386 1.843.067.502-.308 1.408-.485 1.766-.472.357.013 1.061.154 1.782.539.571.197 1.111.115 1.652-.105.541-.221 1.324-1.059 2.238-2.758.347-.79.505-1.217.473-1.282z"/></svg>`;

	const WindowsIcon = `<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" fill="currentColor" viewBox="0 0 16 16"><path d="M6.555 1.375 0 2.237v5.45h6.555V1.375zM0 13.795l6.555.933V8.313H0v5.482zm7.278-5.4.026 6.378L16 16V8.395H7.278zM16 0 7.33 1.244v6.414H16V0z"/></svg>`;

	onMount(() => {
		installInfo = getInstallCommand();

		// Listen for manual copy events (Ctrl+C, Cmd+C, right-click copy)
		const handleCopy = (e: ClipboardEvent) => {
			const selection = window.getSelection();
			const selectedText = selection?.toString().trim();

			// Check if the selected text matches our install command
			if (selectedText && selectedText.includes(installInfo.command)) {
				if (!showModelCommand) {
					setTimeout(() => {
						showModelCommand = true;
					}, 400);
					toast.success('Great! Now run a model');
				}
			}
			// Check if the selected text matches our model command
			else if (selectedText && selectedText.includes(modelCommand)) {
				if (!copiedModel) {
					copiedModel = true;
					toast.success('Copied model command!');
					setTimeout(() => {
						copiedModel = false;
					}, 2000);
				}
			}
		};

		// Listen for text selection to show step 2 even if they don't copy yet
		const handleSelection = () => {
			const selection = window.getSelection();
			const selectedText = selection?.toString().trim();

			// If they've selected the install command text, show step 2
			if (selectedText && selectedText.includes(installInfo.command) && !showModelCommand) {
				setTimeout(() => {
					showModelCommand = true;
				}, 400);
			}
		};

		document.addEventListener('copy', handleCopy);
		document.addEventListener('selectionchange', handleSelection);

		return () => {
			document.removeEventListener('copy', handleCopy);
			document.removeEventListener('selectionchange', handleSelection);
		};
	});

	async function copyCommand() {
		try {
			await navigator.clipboard.writeText(installInfo.command);
			copied = true;
			toast.success('Copied to clipboard!');

			// Show the model command after copying
			setTimeout(() => {
				showModelCommand = true;
			}, 400);

			setTimeout(() => {
				copied = false;
			}, 2000);
		} catch (err) {
			toast.error('Failed to copy to clipboard');
		}
	}

	async function copyModelCommand() {
		try {
			await navigator.clipboard.writeText(modelCommand);
			copiedModel = true;
			toast.success('Copied model command!');
			setTimeout(() => {
				copiedModel = false;
			}, 2000);
		} catch (err) {
			toast.error('Failed to copy to clipboard');
		}
	}

	function getPlatformIcon(label: string) {
		if (label.toLowerCase().includes('windows')) {
			return WindowsIcon;
		} else if (label.toLowerCase().includes('macos')) {
			return AppleIcon;
		}
		return WindowsIcon; // default
	}

	const platformIcon = $derived(getPlatformIcon(installInfo.label));
</script>

<div
	class="border-primary/20 bg-primary/5 hover:border-primary/40 relative w-full rounded-lg border-2 p-4 transition-all duration-300 hover:shadow-lg sm:p-5"
>
	<div class="mb-3 text-center">
		<h3 class="text-foreground text-sm font-semibold sm:text-base">Get Started in Two Steps</h3>
	</div>

	<div class="flex flex-col gap-2.5">
		<!-- Step 1: Install Command -->
		<div class="flex flex-col gap-1.5">
			<span class="text-muted-foreground text-xs font-medium">Step 1: Install Foundry Local</span>
			<div
				class="border-primary/30 bg-background/50 hover:border-primary/50 hover:bg-background group relative flex items-center gap-2 rounded-md border px-3 py-2.5 transition-all duration-300 sm:gap-3"
			>
				<!-- Platform Icon -->
				<div class="flex shrink-0 items-center justify-center" aria-label={installInfo.label}>
					<span class="text-primary">{@html platformIcon}</span>
				</div>

				<!-- Command -->
				<code
					bind:this={commandElement}
					class="text-foreground flex-1 select-all overflow-x-auto font-mono text-xs sm:text-sm"
					style="scrollbar-width: thin;"
				>
					{installInfo.command}
				</code>

				<!-- Copy Button -->
				<button
					type="button"
					onclick={copyCommand}
					class="bg-primary/10 text-primary hover:bg-primary/20 focus:ring-primary flex shrink-0 items-center gap-1.5 rounded px-2 py-1.5 text-xs font-medium transition-all duration-200 hover:scale-105 focus:outline-none focus:ring-2 focus:ring-offset-2 sm:px-2.5"
					aria-label="Copy installation command"
				>
					{#if copied}
						<Check class="size-3.5 sm:size-4" aria-hidden="true" />
						<span class="hidden sm:inline">Copied!</span>
					{:else}
						<Copy class="size-3.5 sm:size-4" aria-hidden="true" />
						<span class="hidden sm:inline">Copy</span>
					{/if}
				</button>
			</div>
		</div>

		<!-- Step 2: Model Command (appears after copying) -->
		<div
			class="flex flex-col gap-1.5 overflow-hidden transition-all duration-700 ease-out"
			style={showModelCommand
				? 'max-height: 120px; opacity: 1;'
				: 'max-height: 0; opacity: 0; margin-top: -10px;'}
		>
			<span class="text-muted-foreground text-xs font-medium">Step 2: Run a model</span>
			<div
				class="border-primary/30 bg-background/50 hover:border-primary/50 hover:bg-background group relative flex items-center gap-2 rounded-md border px-3 py-2.5 transition-all duration-300 sm:gap-3"
			>
				<!-- Terminal Icon -->
				<div class="flex shrink-0 items-center justify-center" aria-label="Run model">
					<Terminal class="text-primary size-5" aria-hidden="true" />
				</div>

				<!-- Command -->
				<code
					bind:this={modelCommandElement}
					class="text-foreground flex-1 select-all overflow-x-auto font-mono text-xs sm:text-sm"
					style="scrollbar-width: thin;"
				>
					{modelCommand}
				</code>
				<!-- Copy Button -->
				<button
					type="button"
					onclick={copyModelCommand}
					class="bg-primary/10 text-primary hover:bg-primary/20 focus:ring-primary flex shrink-0 items-center gap-1.5 rounded px-2 py-1.5 text-xs font-medium transition-all duration-200 hover:scale-105 focus:outline-none focus:ring-2 focus:ring-offset-2 sm:px-2.5"
					aria-label="Copy model run command"
				>
					{#if copiedModel}
						<Check class="size-3.5 sm:size-4" aria-hidden="true" />
						<span class="hidden sm:inline">Copied!</span>
					{:else}
						<Copy class="size-3.5 sm:size-4" aria-hidden="true" />
						<span class="hidden sm:inline">Copy</span>
					{/if}
				</button>
			</div>
		</div>
	</div>
</div>
