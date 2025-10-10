<script lang="ts">
	import { Button, buttonVariants } from '$lib/components/ui/button';
	import * as DropdownMenu from '$lib/components/ui/dropdown-menu';
	import { Download, Github, Copy, Check } from 'lucide-svelte';
	import { toast } from 'svelte-sonner';

	interface Props {
		variant?: 'default' | 'ghost' | 'outline';
		size?: 'default' | 'sm' | 'lg' | 'icon';
		open?: boolean;
	}

	let { variant = 'ghost', size = 'default', open = $bindable(false) }: Props = $props();

	// Platform SVG icons
	const AppleIcon = `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" viewBox="0 0 16 16"><path d="M11.182.008C11.148-.03 9.923.023 8.857 1.18c-1.066 1.156-.902 2.482-.878 2.516.024.034 1.52.087 2.475-1.258.955-1.345.762-2.391.728-2.43zm3.314 11.733c-.048-.096-2.325-1.234-2.113-3.422.212-2.189 1.675-2.789 1.698-2.854.023-.065-.597-.79-1.254-1.157a3.692 3.692 0 0 0-1.563-.434c-.108-.003-.483-.095-1.254.116-.508.139-1.653.589-1.968.607-.316.018-1.256-.522-2.267-.665-.647-.125-1.333.131-1.824.328-.49.196-1.422.754-2.074 2.237-.652 1.482-.311 3.83-.067 4.56.244.729.625 1.924 1.273 2.796.576.984 1.34 1.667 1.659 1.899.319.232 1.219.386 1.843.067.502-.308 1.408-.485 1.766-.472.357.013 1.061.154 1.782.539.571.197 1.111.115 1.652-.105.541-.221 1.324-1.059 2.238-2.758.347-.79.505-1.217.473-1.282z"/></svg>`;
	
	const WindowsIcon = `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" viewBox="0 0 16 16"><path d="M6.555 1.375 0 2.237v5.45h6.555V1.375zM0 13.795l6.555.933V8.313H0v5.482zm7.278-5.4.026 6.378L16 16V8.395H7.278zM16 0 7.33 1.244v6.414H16V0z"/></svg>`;
	
	const PythonIcon = `<svg role="img" width="16" height="16" fill="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><title>Python</title><path d="M14.25.18l.9.2.73.26.59.3.45.32.34.34.25.34.16.33.1.3.04.26.02.2-.01.13V8.5l-.05.63-.13.55-.21.46-.26.38-.3.31-.33.25-.35.19-.35.14-.33.1-.3.07-.26.04-.21.02H8.77l-.69.05-.59.14-.5.22-.41.27-.33.32-.27.35-.2.36-.15.37-.1.35-.07.32-.04.27-.02.21v3.06H3.17l-.21-.03-.28-.07-.32-.12-.35-.18-.36-.26-.36-.36-.35-.46-.32-.59-.28-.73-.21-.88-.14-1.05-.05-1.23.06-1.22.16-1.04.24-.87.32-.71.36-.57.4-.44.42-.33.42-.24.4-.16.36-.1.32-.05.24-.01h.16l.06.01h8.16v-.83H6.18l-.01-2.75-.02-.37.05-.34.11-.31.17-.28.25-.26.31-.23.38-.2.44-.18.51-.15.58-.12.64-.1.71-.06.77-.04.84-.02 1.27.05zm-6.3 1.98l-.23.33-.08.41.08.41.23.34.33.22.41.09.41-.09.33-.22.23-.34.08-.41-.08-.41-.23-.33-.33-.22-.41-.09-.41.09zm13.09 3.95l.28.06.32.12.35.18.36.27.36.35.35.47.32.59.28.73.21.88.14 1.04.05 1.23-.06 1.23-.16 1.04-.24.86-.32.71-.36.57-.4.45-.42.33-.42.24-.4.16-.36.09-.32.05-.24.02-.16-.01h-8.22v.82h5.84l.01 2.76.02.36-.05.34-.11.31-.17.29-.25.25-.31.24-.38.2-.44.17-.51.15-.58.13-.64.09-.71.07-.77.04-.84.01-1.27-.04-1.07-.14-.9-.2-.73-.25-.59-.3-.45-.33-.34-.34-.25-.34-.16-.33-.1-.3-.04-.25-.02-.2.01-.13v-5.34l.05-.64.13-.54.21-.46.26-.38.3-.32.33-.24.35-.2.35-.14.33-.1.3-.06.26-.04.21-.02.13-.01h5.84l.69-.05.59-.14.5-.21.41-.28.33-.32.27-.35.2-.36.15-.36.1-.35.07-.32.04-.28.02-.21V6.07h2.09l.14.01zm-6.47 14.25l-.23.33-.08.41.08.41.23.33.33.23.41.08.41-.08.33-.23.23-.33.08-.41-.08-.41-.23-.33-.33-.23-.41-.08-.41.08z"/></svg>`;
	
	const JavaScriptIcon = `<svg role="img" width="16" height="16" fill="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><title>JavaScript</title><path d="M0 0h24v24H0V0zm22.034 18.276c-.175-1.095-.888-2.015-3.003-2.873-.736-.345-1.554-.585-1.797-1.14-.091-.33-.105-.51-.046-.705.15-.646.915-.84 1.515-.66.39.12.75.42.976.9 1.034-.676 1.034-.676 1.755-1.125-.27-.42-.404-.601-.586-.78-.63-.705-1.469-1.065-2.834-1.034l-.705.089c-.676.165-1.32.525-1.71 1.005-1.14 1.291-.811 3.541.569 4.471 1.365 1.02 3.361 1.244 3.616 2.205.24 1.17-.87 1.545-1.966 1.41-.811-.18-1.26-.586-1.755-1.336l-1.83 1.051c.21.48.45.689.81 1.109 1.74 1.756 6.09 1.666 6.871-1.004.029-.09.24-.705.074-1.65l.046.067zm-8.983-7.245h-2.248c0 1.938-.009 3.864-.009 5.805 0 1.232.063 2.363-.138 2.711-.33.689-1.18.601-1.566.48-.396-.196-.597-.466-.83-.855-.063-.105-.11-.196-.127-.196l-1.825 1.125c.305.63.75 1.172 1.324 1.517.855.51 2.004.675 3.207.405.783-.226 1.458-.691 1.811-1.411.51-.93.402-2.07.397-3.346.012-2.054 0-4.109 0-6.179l.004-.056z"/></svg>`;

	// Commands to copy
	const commands = {
		windows: 'winget install Microsoft.FoundryLocal',
		macos: 'brew tap microsoft/foundrylocal\nbrew install foundrylocal',
		python: 'pip install foundry-local',
		javascript: 'npm install foundry-local'
	};

	let copiedItem = $state<string | null>(null);

	async function copyToClipboard(text: string, label: string) {
		try {
			await navigator.clipboard.writeText(text);
			copiedItem = label;
			toast.success(`Copied to clipboard!`);
			setTimeout(() => {
				copiedItem = null;
			}, 2000);
		} catch (err) {
			toast.error('Failed to copy to clipboard');
		}
	}
</script>

<DropdownMenu.Root bind:open>
	<DropdownMenu.Trigger class={buttonVariants({ variant, size })}>
		<Download class="mr-2 size-4" />
		<span>Download</span>
	</DropdownMenu.Trigger>
	<DropdownMenu.Content align="end" class="w-84">
		<DropdownMenu.Label>Download Foundry Local</DropdownMenu.Label>
				<DropdownMenu.Separator />

		<DropdownMenu.Group>
			<button
				type="button"
				class="relative flex w-full cursor-pointer select-none items-start justify-between rounded-sm px-2 py-1.5 text-left text-sm outline-none transition-colors hover:bg-accent hover:text-accent-foreground"
				onclick={() => copyToClipboard(commands.windows, 'windows')}
			>
				<span class="mt-0.5 inline-flex shrink-0" aria-hidden="true">{@html WindowsIcon}</span>
				<div class="flex flex-1 flex-col gap-1 px-2">
					<span class="font-medium">Windows</span>
					<code class="text-xs text-muted-foreground">{commands.windows}</code>
				</div>
				{#if copiedItem === 'windows'}
					<Check class="size-4 shrink-0 text-green-600" />
				{:else}
					<Copy class="size-4 shrink-0 opacity-50" />
				{/if}
			</button>
			
			<button
				type="button"
				class="relative flex w-full cursor-pointer select-none items-start justify-between rounded-sm px-2 py-1.5 text-left text-sm outline-none transition-colors hover:bg-accent hover:text-accent-foreground"
				onclick={() => copyToClipboard(commands.macos, 'macos')}
			>
				<span class="mt-0.5 inline-flex shrink-0" aria-hidden="true">{@html AppleIcon}</span>
				<div class="flex flex-1 flex-col gap-1 px-2">
					<span class="font-medium">macOS</span>
					<code class="whitespace-pre text-xs text-muted-foreground">{commands.macos}</code>
				</div>
				{#if copiedItem === 'macos'}
					<Check class="size-4 shrink-0 text-green-600" />
				{:else}
					<Copy class="size-4 shrink-0 opacity-50" />
				{/if}
			</button>
		</DropdownMenu.Group>
		
		<DropdownMenu.Separator />
		<DropdownMenu.Label class="text-xs font-normal text-muted-foreground">
			Install SDK
		</DropdownMenu.Label>
		
		<DropdownMenu.Group>
			<button
				type="button"
				class="relative flex w-full cursor-pointer select-none items-start justify-between rounded-sm px-2 py-1.5 text-left text-sm outline-none transition-colors hover:bg-accent hover:text-accent-foreground"
				onclick={() => copyToClipboard(commands.python, 'python')}
			>
				<span class="mt-0.5 inline-flex shrink-0" aria-hidden="true">{@html PythonIcon}</span>
				<div class="flex flex-1 flex-col gap-1 px-2">
					<span class="font-medium">Python SDK</span>
					<code class="text-xs text-muted-foreground">{commands.python}</code>
				</div>
				{#if copiedItem === 'python'}
					<Check class="size-4 shrink-0 text-green-600" />
				{:else}
					<Copy class="size-4 shrink-0 opacity-50" />
				{/if}
			</button>
			
			<button
				type="button"
				class="relative flex w-full cursor-pointer select-none items-start justify-between rounded-sm px-2 py-1.5 text-left text-sm outline-none transition-colors hover:bg-accent hover:text-accent-foreground"
				onclick={() => copyToClipboard(commands.javascript, 'javascript')}
			>
				<span class="mt-0.5 inline-flex shrink-0" aria-hidden="true">{@html JavaScriptIcon}</span>
				<div class="flex flex-1 flex-col gap-1 px-2">
					<span class="font-medium">JavaScript SDK</span>
					<code class="text-xs text-muted-foreground">{commands.javascript}</code>
				</div>
				{#if copiedItem === 'javascript'}
					<Check class="size-4 shrink-0 text-green-600" />
				{:else}
					<Copy class="size-4 shrink-0 opacity-50" />
				{/if}
			</button>
		</DropdownMenu.Group>
		
		<DropdownMenu.Separator />
		
		<DropdownMenu.Group>
			<DropdownMenu.Item>
				<a
					href="https://github.com/microsoft/foundry-local/releases"
					target="_blank"
					rel="noopener noreferrer"
					class="flex w-full items-center"
				>
					<Github class="mr-2 size-4" />
					<span>All Releases</span>
				</a>
			</DropdownMenu.Item>
		</DropdownMenu.Group>
	</DropdownMenu.Content>
</DropdownMenu.Root>

<style>
	:global(.dark) code {
		color: hsl(var(--muted-foreground));
	}
</style>
