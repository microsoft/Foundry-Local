<script lang="ts">
	import { buttonVariants } from '$lib/components/ui/button';
	import * as DropdownMenu from '$lib/components/ui/dropdown-menu';
	import { Download, Copy, Check } from 'lucide-svelte';
	import { toast } from 'svelte-sonner';
	import { IsMobile } from '$lib/hooks/is-mobile.svelte';

	interface Props {
		variant?: 'default' | 'ghost' | 'outline';
		size?: 'default' | 'sm' | 'lg' | 'icon';
		open?: boolean;
		class?: string;
	}

	type CommandItem = {
		id: string;
		label: string;
		command: string;
		badge: string;
		ariaLabel: string;
	};

	type CommandSection = {
		title: string;
		description: string;
		items: CommandItem[];
	};

	let {
		variant = 'ghost',
		size = 'default',
		open = $bindable(false),
		class: className = ''
	}: Props = $props();

	const isMobile = new IsMobile();

	const sections: CommandSection[] = [
		{
			title: 'Start with an SDK',
			description: 'Use this path when you are building an application with local inference.',
			items: [
				{
					id: 'python',
					label: 'Python SDK',
					command: 'pip install foundry-local-sdk',
					badge: 'PY',
					ariaLabel: 'Copy Python SDK installation command'
				},
				{
					id: 'javascript',
					label: 'JavaScript SDK',
					command: 'npm install foundry-local-sdk',
					badge: 'JS',
					ariaLabel: 'Copy JavaScript SDK installation command'
				},
				{
					id: 'csharp',
					label: 'C# SDK',
					command: 'dotnet add package Microsoft.AI.Foundry.Local',
					badge: 'C#',
					ariaLabel: 'Copy C# SDK installation command'
				},
				{
					id: 'rust',
					label: 'Rust SDK',
					command: 'cargo add foundry-local-sdk',
					badge: 'RS',
					ariaLabel: 'Copy Rust SDK installation command'
				}
			]
		},
		{
			title: 'Windows SDK acceleration',
			description: 'Use WinML packages when you want Windows hardware acceleration from the SDK.',
			items: [
				{
					id: 'python_winml',
					label: 'Python SDK (WinML)',
					command: 'pip install foundry-local-sdk-winml',
					badge: 'PY',
					ariaLabel: 'Copy Python WinML SDK installation command'
				},
				{
					id: 'javascript_winml',
					label: 'JavaScript SDK (WinML)',
					command: 'npm install foundry-local-sdk-winml',
					badge: 'JS',
					ariaLabel: 'Copy JavaScript WinML SDK installation command'
				},
				{
					id: 'csharp_winml',
					label: 'C# SDK (WinML)',
					command: 'dotnet add package Microsoft.AI.Foundry.Local.WinML',
					badge: 'C#',
					ariaLabel: 'Copy C# WinML SDK installation command'
				},
				{
					id: 'rust_winml',
					label: 'Rust SDK (WinML)',
					command: 'cargo add foundry-local-sdk --features winml',
					badge: 'RS',
					ariaLabel: 'Copy Rust WinML SDK installation command'
				}
			]
		},
		{
			title: 'Optional CLI tools',
			description: 'Use the CLI to explore models or test prompts outside your app.',
			items: [
				{
					id: 'windows',
					label: 'Windows CLI',
					command: 'winget install Microsoft.FoundryLocal',
					badge: 'WIN',
					ariaLabel: 'Copy Windows CLI installation command'
				},
				{
					id: 'macos',
					label: 'macOS CLI',
					command: 'brew install microsoft/foundrylocal/foundrylocal',
					badge: 'MAC',
					ariaLabel: 'Copy macOS CLI installation command'
				}
			]
		}
	];

	let copiedItem = $state<string | null>(null);

	async function copyToClipboard(text: string, label: string) {
		try {
			await navigator.clipboard.writeText(text);
			copiedItem = label;
			toast.success('Copied to clipboard!');
			setTimeout(() => {
				copiedItem = null;
			}, 2000);
		} catch (err) {
			toast.error('Failed to copy to clipboard');
		}
	}
</script>

<DropdownMenu.Root bind:open>
	<DropdownMenu.Trigger
		class={`${buttonVariants({ variant, size })} ${className} group`}
		aria-label="Install Foundry Local SDK or CLI"
	>
		<Download
			class="download-hover-icon mr-2 size-4 transition-transform duration-300"
			aria-hidden="true"
		/>
		<span>Install</span>
	</DropdownMenu.Trigger>
	<DropdownMenu.Content
		align="end"
		class={isMobile.current ? 'mx-4 w-[calc(100vw-2rem)]' : 'w-[30rem]'}
		aria-label="Install options"
	>
		<DropdownMenu.Label>Install Foundry Local</DropdownMenu.Label>
		<p class="text-muted-foreground px-2 pb-2 text-xs leading-relaxed">
			Choose an SDK to embed local AI in your app. The CLI is available as secondary developer
			tooling.
		</p>

		{#each sections as section}
			<DropdownMenu.Separator />
			<DropdownMenu.Label class="text-muted-foreground text-xs font-normal">
				{section.title}
			</DropdownMenu.Label>
			<p class="text-muted-foreground px-2 pb-1 text-[11px] leading-relaxed">
				{section.description}
			</p>

			<DropdownMenu.Group>
				{#each section.items as item}
					<button
						type="button"
						class="hover:bg-accent hover:text-accent-foreground focus:bg-accent focus:text-accent-foreground relative flex min-h-[44px] w-full cursor-pointer items-start justify-between rounded-sm px-2 py-2.5 text-left text-sm transition-colors outline-none select-none"
						onclick={() => copyToClipboard(item.command, item.id)}
						aria-label={item.ariaLabel}
					>
						<span
							class="bg-primary/10 text-primary mt-0.5 inline-flex min-w-8 shrink-0 items-center justify-center rounded-md px-1.5 py-1 font-mono text-[10px] font-bold"
							aria-hidden="true"
						>
							{item.badge}
						</span>
						<div class="flex flex-1 flex-col gap-1 px-2">
							<span class="font-medium">{item.label}</span>
							<code class="text-muted-foreground text-xs break-all">{item.command}</code>
						</div>
						{#if copiedItem === item.id}
							<Check class="text-primary size-4 shrink-0" aria-hidden="true" />
							<span class="sr-only">Copied</span>
						{:else}
							<Copy class="size-4 shrink-0 opacity-50" aria-hidden="true" />
						{/if}
					</button>
				{/each}
			</DropdownMenu.Group>
		{/each}

		<DropdownMenu.Separator />
		<DropdownMenu.Group>
			<DropdownMenu.Item>
				<a
					href="https://github.com/microsoft/foundry-local/releases"
					target="_blank"
					rel="noopener noreferrer"
					class="flex w-full items-center"
					aria-label="View all releases on GitHub (opens in new tab)"
				>
					<span>All releases</span>
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
