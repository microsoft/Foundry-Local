<script lang="ts">
	import { Button, buttonVariants } from '$lib/components/ui/button';
	import * as DropdownMenu from '$lib/components/ui/dropdown-menu';
	import { Download, Github, Copy, Check } from 'lucide-svelte';
	import { toast } from 'svelte-sonner';
	import { IsMobile } from '$lib/hooks/is-mobile.svelte';

	interface Props {
		variant?: 'default' | 'ghost' | 'outline';
		size?: 'default' | 'sm' | 'lg' | 'icon';
		open?: boolean;
		class?: string;
	}

	let {
		variant = 'ghost',
		size = 'default',
		open = $bindable(false),
		class: className = ''
	}: Props = $props();

	const isMobile = new IsMobile();

	// Platform SVG icons
	const AppleIcon = `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" viewBox="0 0 16 16"><path d="M11.182.008C11.148-.03 9.923.023 8.857 1.18c-1.066 1.156-.902 2.482-.878 2.516.024.034 1.52.087 2.475-1.258.955-1.345.762-2.391.728-2.43zm3.314 11.733c-.048-.096-2.325-1.234-2.113-3.422.212-2.189 1.675-2.789 1.698-2.854.023-.065-.597-.79-1.254-1.157a3.692 3.692 0 0 0-1.563-.434c-.108-.003-.483-.095-1.254.116-.508.139-1.653.589-1.968.607-.316.018-1.256-.522-2.267-.665-.647-.125-1.333.131-1.824.328-.49.196-1.422.754-2.074 2.237-.652 1.482-.311 3.83-.067 4.56.244.729.625 1.924 1.273 2.796.576.984 1.34 1.667 1.659 1.899.319.232 1.219.386 1.843.067.502-.308 1.408-.485 1.766-.472.357.013 1.061.154 1.782.539.571.197 1.111.115 1.652-.105.541-.221 1.324-1.059 2.238-2.758.347-.79.505-1.217.473-1.282z"/></svg>`;

	const WindowsIcon = `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" viewBox="0 0 16 16"><path d="M6.555 1.375 0 2.237v5.45h6.555V1.375zM0 13.795l6.555.933V8.313H0v5.482zm7.278-5.4.026 6.378L16 16V8.395H7.278zM16 0 7.33 1.244v6.414H16V0z"/></svg>`;

	const PythonIcon = `<svg role="img" width="16" height="16" fill="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><title>Python</title><path d="M14.25.18l.9.2.73.26.59.3.45.32.34.34.25.34.16.33.1.3.04.26.02.2-.01.13V8.5l-.05.63-.13.55-.21.46-.26.38-.3.31-.33.25-.35.19-.35.14-.33.1-.3.07-.26.04-.21.02H8.77l-.69.05-.59.14-.5.22-.41.27-.33.32-.27.35-.2.36-.15.37-.1.35-.07.32-.04.27-.02.21v3.06H3.17l-.21-.03-.28-.07-.32-.12-.35-.18-.36-.26-.36-.36-.35-.46-.32-.59-.28-.73-.21-.88-.14-1.05-.05-1.23.06-1.22.16-1.04.24-.87.32-.71.36-.57.4-.44.42-.33.42-.24.4-.16.36-.1.32-.05.24-.01h.16l.06.01h8.16v-.83H6.18l-.01-2.75-.02-.37.05-.34.11-.31.17-.28.25-.26.31-.23.38-.2.44-.18.51-.15.58-.12.64-.1.71-.06.77-.04.84-.02 1.27.05zm-6.3 1.98l-.23.33-.08.41.08.41.23.34.33.22.41.09.41-.09.33-.22.23-.34.08-.41-.08-.41-.23-.33-.33-.22-.41-.09-.41.09zm13.09 3.95l.28.06.32.12.35.18.36.27.36.35.35.47.32.59.28.73.21.88.14 1.04.05 1.23-.06 1.23-.16 1.04-.24.86-.32.71-.36.57-.4.45-.42.33-.42.24-.4.16-.36.09-.32.05-.24.02-.16-.01h-8.22v.82h5.84l.01 2.76.02.36-.05.34-.11.31-.17.29-.25.25-.31.24-.38.2-.44.17-.51.15-.58.13-.64.09-.71.07-.77.04-.84.01-1.27-.04-1.07-.14-.9-.2-.73-.25-.59-.3-.45-.33-.34-.34-.25-.34-.16-.33-.1-.3-.04-.25-.02-.2.01-.13v-5.34l.05-.64.13-.54.21-.46.26-.38.3-.32.33-.24.35-.2.35-.14.33-.1.3-.06.26-.04.21-.02.13-.01h5.84l.69-.05.59-.14.5-.21.41-.28.33-.32.27-.35.2-.36.15-.36.1-.35.07-.32.04-.28.02-.21V6.07h2.09l.14.01zm-6.47 14.25l-.23.33-.08.41.08.41.23.33.33.23.41.08.41-.08.33-.23.23-.33.08-.41-.08-.41-.23-.33-.33-.23-.41-.08-.41.08z"/></svg>`;

	const JavaScriptIcon = `<svg role="img" width="16" height="16" fill="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><title>JavaScript</title><path d="M0 0h24v24H0V0zm22.034 18.276c-.175-1.095-.888-2.015-3.003-2.873-.736-.345-1.554-.585-1.797-1.14-.091-.33-.105-.51-.046-.705.15-.646.915-.84 1.515-.66.39.12.75.42.976.9 1.034-.676 1.034-.676 1.755-1.125-.27-.42-.404-.601-.586-.78-.63-.705-1.469-1.065-2.834-1.034l-.705.089c-.676.165-1.32.525-1.71 1.005-1.14 1.291-.811 3.541.569 4.471 1.365 1.02 3.361 1.244 3.616 2.205.24 1.17-.87 1.545-1.966 1.41-.811-.18-1.26-.586-1.755-1.336l-1.83 1.051c.21.48.45.689.81 1.109 1.74 1.756 6.09 1.666 6.871-1.004.029-.09.24-.705.074-1.65l.046.067zm-8.983-7.245h-2.248c0 1.938-.009 3.864-.009 5.805 0 1.232.063 2.363-.138 2.711-.33.689-1.18.601-1.566.48-.396-.196-.597-.466-.83-.855-.063-.105-.11-.196-.127-.196l-1.825 1.125c.305.63.75 1.172 1.324 1.517.855.51 2.004.675 3.207.405.783-.226 1.458-.691 1.811-1.411.51-.93.402-2.07.397-3.346.012-2.054 0-4.109 0-6.179l.004-.056z"/></svg>`;

	const CSharpIcon = `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 50 50" fill="currentColor"><path d="M 25 2 C 24.285156 2 23.570313 2.179688 22.933594 2.539063 L 6.089844 12.003906 C 4.800781 12.726563 4 14.082031 4 15.535156 L 4 34.464844 C 4 35.917969 4.800781 37.273438 6.089844 37.996094 L 22.933594 47.460938 C 23.570313 47.820313 24.285156 48 25 48 C 25.714844 48 26.429688 47.820313 27.066406 47.460938 L 43.910156 38 C 45.199219 37.273438 46 35.917969 46 34.464844 L 46 15.535156 C 46 14.082031 45.199219 12.726563 43.910156 12.003906 L 27.066406 2.539063 C 26.429688 2.179688 25.714844 2 25 2 Z M 25 13 C 28.78125 13 32.277344 14.753906 34.542969 17.738281 L 30.160156 20.277344 C 28.84375 18.835938 26.972656 18 25 18 C 21.140625 18 18 21.140625 18 25 C 18 28.859375 21.140625 32 25 32 C 26.972656 32 28.84375 31.164063 30.160156 29.722656 L 34.542969 32.261719 C 32.277344 35.246094 28.78125 37 25 37 C 18.382813 37 13 31.617188 13 25 C 13 18.382813 18.382813 13 25 13 Z M 35 20 L 37 20 L 37 22 L 39 22 L 39 20 L 41 20 L 41 22 L 43 22 L 43 24 L 41 24 L 41 26 L 43 26 L 43 28 L 41 28 L 41 30 L 39 30 L 39 28 L 37 28 L 37 30 L 35 30 L 35 28 L 33 28 L 33 26 L 35 26 L 35 24 L 33 24 L 33 22 L 35 22 Z M 37 24 L 37 26 L 39 26 L 39 24 Z"/></svg>`;

	const RustIcon = `<svg role="img" width="16" height="16" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg" fill="currentColor"><path d="M23.8346 11.7033l-1.0073-.6236a13.7268 13.7268 0 00-.0283-.2936l.8656-.8069a.3483.3483 0 00-.1154-.578l-1.1066-.414a8.4958 8.4958 0 00-.087-.2856l.6904-.9587a.3462.3462 0 00-.2257-.5446l-1.1663-.1894a9.3574 9.3574 0 00-.1407-.2622l.49-1.0761a.3437.3437 0 00-.0274-.3361.3486.3486 0 00-.3006-.154l-1.1845.0416a6.7444 6.7444 0 00-.1873-.2268l.2723-1.153a.3472.3472 0 00-.417-.4172l-1.1532.2724a14.0183 14.0183 0 00-.2278-.1873l.0415-1.1845a.3442.3442 0 00-.49-.328l-1.076.491c-.0872-.0476-.1742-.0952-.2623-.1407l-.1903-1.1673A.3483.3483 0 0016.256.955l-.9597.6905a8.4867 8.4867 0 00-.2855-.086l-.414-1.1066a.3483.3483 0 00-.5781-.1154l-.8069.8666a9.2936 9.2936 0 00-.2936-.0284L12.2946.1683a.3462.3462 0 00-.5892 0l-.6236 1.0073a13.7383 13.7383 0 00-.2936.0284L9.9803.3374a.3462.3462 0 00-.578.1154l-.4141 1.1065c-.0962.0274-.1903.0567-.2855.086L7.744.955a.3483.3483 0 00-.5447.2258L7.009 2.348a9.3574 9.3574 0 00-.2622.1407l-1.0762-.491a.3462.3462 0 00-.49.328l.0416 1.1845a7.9826 7.9826 0 00-.2278.1873L3.8413 3.425a.3472.3472 0 00-.4171.4171l.2713 1.1531c-.0628.075-.1255.1509-.1863.2268l-1.1845-.0415a.3462.3462 0 00-.328.49l.491 1.0761a9.167 9.167 0 00-.1407.2622l-1.1662.1894a.3483.3483 0 00-.2258.5446l.6904.9587a13.303 13.303 0 00-.087.2855l-1.1065.414a.3483.3483 0 00-.1155.5781l.8656.807a9.2936 9.2936 0 00-.0283.2935l-1.0073.6236a.3442.3442 0 000 .5892l1.0073.6236c.008.0982.0182.1964.0283.2936l-.8656.8079a.3462.3462 0 00.1155.578l1.1065.4141c.0273.0962.0567.1914.087.2855l-.6904.9587a.3452.3452 0 00.2268.5447l1.1662.1893c.0456.088.0922.1751.1408.2622l-.491 1.0762a.3462.3462 0 00.328.49l1.1834-.0415c.0618.0769.1235.1528.1873.2277l-.2713 1.1541a.3462.3462 0 00.4171.4161l1.153-.2713c.075.0638.151.1255.2279.1863l-.0415 1.1845a.3442.3442 0 00.49.327l1.0761-.49c.087.0486.1741.0951.2622.1407l.1903 1.1662a.3483.3483 0 00.5447.2268l.9587-.6904a9.299 9.299 0 00.2855.087l.414 1.1066a.3452.3452 0 00.5781.1154l.8079-.8656c.0972.0111.1954.0203.2936.0294l.6236 1.0073a.3472.3472 0 00.5892 0l.6236-1.0073c.0982-.0091.1964-.0183.2936-.0294l.8069.8656a.3483.3483 0 00.578-.1154l.4141-1.1066a8.4626 8.4626 0 00.2855-.087l.9587.6904a.3452.3452 0 00.5447-.2268l.1903-1.1662c.088-.0456.1751-.0931.2622-.1407l1.0762.49a.3472.3472 0 00.49-.327l-.0415-1.1845a6.7267 6.7267 0 00.2267-.1863l1.1531.2713a.3472.3472 0 00.4171-.416l-.2713-1.1542c.0628-.0749.1255-.1508.1863-.2278l1.1845.0415a.3442.3442 0 00.328-.49l-.49-1.076c.0475-.0872.0951-.1742.1407-.2623l1.1662-.1893a.3483.3483 0 00.2258-.5447l-.6904-.9587.087-.2855 1.1066-.414a.3462.3462 0 00.1154-.5781l-.8656-.8079c.0101-.0972.0202-.1954.0283-.2936l1.0073-.6236a.3442.3442 0 000-.5892zm-6.7413 8.3551a.7138.7138 0 01.2986-1.396.714.714 0 11-.2997 1.396zm-.3422-2.3142a.649.649 0 00-.7715.5l-.3573 1.6685c-1.1035.501-2.3285.7795-3.6193.7795a8.7368 8.7368 0 01-3.6951-.814l-.3574-1.6684a.648.648 0 00-.7714-.499l-1.473.3158a8.7216 8.7216 0 01-.7613-.898h7.1676c.081 0 .1356-.0141.1356-.088v-2.536c0-.074-.0536-.0881-.1356-.0881h-2.0966v-1.6077h2.2677c.2065 0 1.1065.0587 1.394 1.2088.0901.3533.2875 1.5044.4232 1.8729.1346.413.6833 1.2381 1.2685 1.2381h3.5716a.7492.7492 0 00.1296-.0131 8.7874 8.7874 0 01-.8119.9526zM6.8369 20.024a.714.714 0 11-.2997-1.396.714.714 0 01.2997 1.396zM4.1177 8.9972a.7137.7137 0 11-1.304.5791.7137.7137 0 011.304-.579zm-.8352 1.9813l1.5347-.6824a.65.65 0 00.33-.8585l-.3158-.7147h1.2432v5.6025H3.5669a8.7753 8.7753 0 01-.2834-3.348zm6.7343-.5437V8.7836h2.9601c.153 0 1.0792.1772 1.0792.8697 0 .575-.7107.7815-1.2948.7815zm10.7574 1.4862c0 .2187-.008.4363-.0243.651h-.9c-.09 0-.1265.0586-.1265.1477v.413c0 .973-.5487 1.1846-1.0296 1.2382-.4576.0517-.9648-.1913-1.0275-.4717-.2704-1.5186-.7198-1.8436-1.4305-2.4034.8817-.5599 1.799-1.386 1.799-2.4915 0-1.1936-.819-1.9458-1.3769-2.3153-.7825-.5163-1.6491-.6195-1.883-.6195H5.4682a8.7651 8.7651 0 014.907-2.7699l1.0974 1.151a.648.648 0 00.9182.0213l1.227-1.1743a8.7753 8.7753 0 016.0044 4.2762l-.8403 1.8982a.652.652 0 00.33.8585l1.6178.7188c.0283.2875.0425.577.0425.8717zm-9.3006-9.5993a.7128.7128 0 11.984 1.0316.7137.7137 0 01-.984-1.0316zm8.3389 6.71a.7107.7107 0 01.9395-.3625.7137.7137 0 11-.9405.3635z"/></svg>`;

	// Commands to copy
	const commands = {
		windows: 'winget install Microsoft.FoundryLocal',
		macos: 'brew install microsoft/foundrylocal/foundrylocal',
		python: 'pip install foundry-local-sdk',
		javascript: 'npm install foundry-local-sdk',
		csharp: 'dotnet add package Microsoft.AI.Foundry.Local',
		rust: 'cargo add foundry-local'
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
	<DropdownMenu.Trigger
		class={`${buttonVariants({ variant, size })} ${className} group`}
		aria-label="Download Foundry Local for your platform"
	>
		<Download
			class="download-hover-icon mr-2 size-4 transition-transform duration-300"
			aria-hidden="true"
		/>
		<span>Download</span>
	</DropdownMenu.Trigger>
	<DropdownMenu.Content
		align="end"
		class={isMobile.current ? 'mx-4 w-[calc(100vw-2rem)]' : 'w-[28rem]'}
		aria-label="Download options"
	>
		<DropdownMenu.Label>Download Foundry Local</DropdownMenu.Label>
		<DropdownMenu.Separator />

		<DropdownMenu.Group>
			<button
				type="button"
				class="hover:bg-accent hover:text-accent-foreground focus:bg-accent focus:text-accent-foreground relative flex min-h-[44px] w-full cursor-pointer select-none items-start justify-between rounded-sm px-2 py-2.5 text-left text-sm outline-none transition-colors"
				onclick={() => copyToClipboard(commands.windows, 'windows')}
				aria-label="Copy Windows installation command"
			>
				<span class="mt-0.5 inline-flex shrink-0" aria-hidden="true">{@html WindowsIcon}</span>
				<div class="flex flex-1 flex-col gap-1 px-2">
					<span class="font-medium">Windows</span>
					<code class="text-muted-foreground break-all text-xs">{commands.windows}</code>
				</div>
				{#if copiedItem === 'windows'}
					<Check class="size-4 shrink-0 text-green-600" aria-hidden="true" />
					<span class="sr-only">Copied</span>
				{:else}
					<Copy class="size-4 shrink-0 opacity-50" aria-hidden="true" />
				{/if}
			</button>

			<button
				type="button"
				class="hover:bg-accent hover:text-accent-foreground focus:bg-accent focus:text-accent-foreground relative flex min-h-[44px] w-full cursor-pointer select-none items-start justify-between rounded-sm px-2 py-2.5 text-left text-sm outline-none transition-colors"
				onclick={() => copyToClipboard(commands.macos, 'macos')}
				aria-label="Copy macOS installation command"
			>
				<span class="mt-0.5 inline-flex shrink-0" aria-hidden="true">{@html AppleIcon}</span>
				<div class="flex flex-1 flex-col gap-1 px-2">
					<span class="font-medium">macOS</span>
					<code class="text-muted-foreground whitespace-pre break-all text-xs"
						>{commands.macos}</code
					>
				</div>
				{#if copiedItem === 'macos'}
					<Check class="size-4 shrink-0 text-green-600" />
				{:else}
					<Copy class="size-4 shrink-0 opacity-50" />
				{/if}
			</button>
		</DropdownMenu.Group>

		<DropdownMenu.Separator />
		<DropdownMenu.Label class="text-muted-foreground text-xs font-normal">
			Install SDK
		</DropdownMenu.Label>

		<DropdownMenu.Group>
			<button
				type="button"
				class="hover:bg-accent hover:text-accent-foreground focus:bg-accent focus:text-accent-foreground relative flex min-h-[44px] w-full cursor-pointer select-none items-start justify-between rounded-sm px-2 py-2.5 text-left text-sm outline-none transition-colors"
				onclick={() => copyToClipboard(commands.python, 'python')}
				aria-label="Copy Python SDK installation command"
			>
				<span class="mt-0.5 inline-flex shrink-0" aria-hidden="true">{@html PythonIcon}</span>
				<div class="flex flex-1 flex-col gap-1 px-2">
					<span class="font-medium">Python SDK</span>
					<code class="text-muted-foreground break-all text-xs">{commands.python}</code>
				</div>
				{#if copiedItem === 'python'}
					<Check class="size-4 shrink-0 text-green-600" aria-hidden="true" />
					<span class="sr-only">Copied</span>
				{:else}
					<Copy class="size-4 shrink-0 opacity-50" aria-hidden="true" />
				{/if}
			</button>

			<button
				type="button"
				class="hover:bg-accent hover:text-accent-foreground focus:bg-accent focus:text-accent-foreground relative flex min-h-[44px] w-full cursor-pointer select-none items-start justify-between rounded-sm px-2 py-2.5 text-left text-sm outline-none transition-colors"
				onclick={() => copyToClipboard(commands.javascript, 'javascript')}
				aria-label="Copy JavaScript SDK installation command"
			>
				<span class="mt-0.5 inline-flex shrink-0" aria-hidden="true">{@html JavaScriptIcon}</span>
				<div class="flex flex-1 flex-col gap-1 px-2">
					<span class="font-medium">JavaScript SDK</span>
					<code class="text-muted-foreground break-all text-xs">{commands.javascript}</code>
				</div>
				{#if copiedItem === 'javascript'}
					<Check class="size-4 shrink-0 text-green-600" aria-hidden="true" />
					<span class="sr-only">Copied</span>
				{:else}
					<Copy class="size-4 shrink-0 opacity-50" aria-hidden="true" />
				{/if}
			</button>

			<button
				type="button"
				class="hover:bg-accent hover:text-accent-foreground focus:bg-accent focus:text-accent-foreground relative flex min-h-[44px] w-full cursor-pointer select-none items-start justify-between rounded-sm px-2 py-2.5 text-left text-sm outline-none transition-colors"
				onclick={() => copyToClipboard(commands.csharp, 'csharp')}
				aria-label="Copy C# SDK installation command"
			>
				<span class="mt-0.5 inline-flex shrink-0" aria-hidden="true">{@html CSharpIcon}</span>
				<div class="flex flex-1 flex-col gap-1 px-2">
					<span class="font-medium">C# SDK</span>
					<code class="text-muted-foreground break-all text-xs">{commands.csharp}</code>
				</div>
				{#if copiedItem === 'csharp'}
					<Check class="size-4 shrink-0 text-green-600" aria-hidden="true" />
					<span class="sr-only">Copied</span>
				{:else}
					<Copy class="size-4 shrink-0 opacity-50" aria-hidden="true" />
				{/if}
			</button>

			<button
				type="button"
				class="hover:bg-accent hover:text-accent-foreground focus:bg-accent focus:text-accent-foreground relative flex min-h-[44px] w-full cursor-pointer select-none items-start justify-between rounded-sm px-2 py-2.5 text-left text-sm outline-none transition-colors"
				onclick={() => copyToClipboard(commands.rust, 'rust')}
				aria-label="Copy Rust SDK installation command"
			>
				<span class="mt-0.5 inline-flex shrink-0" aria-hidden="true">{@html RustIcon}</span>
				<div class="flex flex-1 flex-col gap-1 px-2">
					<span class="font-medium">Rust SDK</span>
					<code class="text-muted-foreground break-all text-xs">{commands.rust}</code>
				</div>
				{#if copiedItem === 'rust'}
					<Check class="size-4 shrink-0 text-green-600" aria-hidden="true" />
					<span class="sr-only">Copied</span>
				{:else}
					<Copy class="size-4 shrink-0 opacity-50" aria-hidden="true" />
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
					aria-label="View all releases on GitHub"
				>
					<Github class="mr-2 size-4" aria-hidden="true" />
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
