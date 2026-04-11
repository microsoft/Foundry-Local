<script lang="ts">
	import { Copy, Check, Terminal } from 'lucide-svelte';
	import { toast } from 'svelte-sonner';
	import { onMount } from 'svelte';
	import { detectPlatform } from '$lib/utils/platform';

	let copied = $state(false);
	let copiedCode = $state(false);
	let showCodeSnippet = $state(false);
	let commandElement: HTMLElement;
	let activeTab = $state<'python' | 'javascript' | 'csharp' | 'rust'>('python');

	const sdkCommands = {
		python: 'pip install foundry-local-sdk',
		javascript: 'npm install foundry-local-sdk',
		csharp: 'dotnet add package Microsoft.AI.Foundry.Local',
		rust: 'cargo add foundry-local-sdk'
	};

	const codeSnippets: Record<string, string> = {
		python: `from foundry_local_sdk import Configuration, FoundryLocalManager
config = Configuration(app_name="my-app")
FoundryLocalManager.initialize(config)
model = FoundryLocalManager.instance.catalog.get_model("qwen2.5-0.5b")
model.download(); model.load()
response = model.get_chat_client().complete_chat(
    [{"role": "user", "content": "Hello!"}])
print(response.choices[0].message.content)`,
		javascript: `import { FoundryLocalManager } from 'foundry-local-sdk';
const manager = FoundryLocalManager.create({ appName: 'my-app' });
const model = await manager.catalog.getModel('qwen2.5-0.5b');
await model.download(); await model.load();
const chat = model.createChatClient();
const res = await chat.completeChat(
    [{ role: 'user', content: 'Hello!' }]);
console.log(res.choices[0]?.message?.content);`,
		csharp: `using Microsoft.AI.Foundry.Local;
using Microsoft.Extensions.Logging.Abstractions;
await FoundryLocalManager.CreateAsync(
    new Configuration { AppName = "my-app" },
    NullLogger.Instance);
var catalog = await FoundryLocalManager.Instance.GetCatalogAsync();
var model = await catalog.GetModelAsync("qwen2.5-0.5b");
await model.DownloadAsync(); await model.LoadAsync();
var client = await model.GetChatClientAsync();
var res = await client.CompleteChatAsync(new[] {
    new ChatMessage { Role="user", Content="Hello!" }});
Console.WriteLine(res.Choices![0].Message.Content);`,
		rust: `use foundry_local_sdk::*;
let manager = FoundryLocalManager::create(
    FoundryLocalConfig::new("my-app"))?;
let model = manager.catalog().get_model("qwen2.5-0.5b").await?;
model.download(None::<fn(f64)>).await?;
model.load().await?;
let client = model.create_chat_client();
let msgs = vec![ChatCompletionRequestUserMessage
    ::from("Hello!").into()];
let res = client.complete_chat(&msgs, None).await?;
println!("{}", res.choices[0].message.content
    .as_ref().unwrap());`
	};

	const tabLabels: Record<string, string> = {
		python: 'Python',
		javascript: 'JS',
		csharp: 'C#',
		rust: 'Rust'
	};

	onMount(() => {
		const platform = detectPlatform();
		if (platform === 'windows') {
			activeTab = 'csharp';
		} else {
			activeTab = 'python';
		}
	});

	$effect(() => {
		// Reactively derive current command from active tab
		void activeTab;
	});

	let currentCommand = $derived(sdkCommands[activeTab]);

	async function copyCommand() {
		try {
			await navigator.clipboard.writeText(currentCommand);
			copied = true;
			toast.success('Copied to clipboard!');
			setTimeout(() => { showCodeSnippet = true; }, 400);
			setTimeout(() => { copied = false; }, 2000);
		} catch (err) {
			toast.error('Failed to copy to clipboard');
		}
	}

	async function copyCodeSnippet() {
		try {
			await navigator.clipboard.writeText(codeSnippets[activeTab]);
			copiedCode = true;
			toast.success('Copied code snippet!');
			setTimeout(() => { copiedCode = false; }, 2000);
		} catch (err) {
			toast.error('Failed to copy to clipboard');
		}
	}
</script>

<div
	class="border-primary/20 bg-primary/5 hover:border-primary/40 relative w-full rounded-lg border-2 p-4 transition-all duration-300 hover:shadow-lg sm:p-5"
>
	<div class="mb-3 text-center">
		<h3 class="text-foreground text-sm font-semibold sm:text-base">Get Started: Install the SDK</h3>
	</div>

	<!-- Language Tabs -->
	<div class="mb-3 flex justify-center gap-1">
		{#each Object.entries(tabLabels) as [key, label]}
			<button
				type="button"
				class="rounded-md px-3 py-1.5 text-xs font-medium transition-all duration-200 {activeTab === key
					? 'bg-primary text-primary-foreground shadow-sm'
					: 'text-muted-foreground hover:text-foreground hover:bg-muted'}"
				onclick={() => { activeTab = key as 'python' | 'javascript' | 'csharp' | 'rust'; showCodeSnippet = false; }}
			>
				{label}
			</button>
		{/each}
	</div>

	<div class="flex flex-col gap-2.5">
		<!-- Install Command -->
		<div class="flex flex-col gap-1.5">
			<span class="text-muted-foreground text-xs font-medium">Install the SDK</span>
			<div
				class="border-primary/30 bg-background/50 hover:border-primary/50 hover:bg-background group relative flex items-center gap-2 rounded-md border px-3 py-2.5 transition-all duration-300 sm:gap-3"
			>
				<div class="flex shrink-0 items-center justify-center">
					<Terminal class="text-primary size-5" aria-hidden="true" />
				</div>
				<code
					bind:this={commandElement}
					class="text-foreground flex-1 select-all overflow-x-auto font-mono text-xs sm:text-sm"
					style="scrollbar-width: thin;"
				>
					{currentCommand}
				</code>
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

		<!-- Code Snippet (appears after copying) -->
		<div
			class="flex flex-col gap-1.5 overflow-hidden transition-all duration-700 ease-out"
			style={showCodeSnippet
				? 'max-height: 300px; opacity: 1;'
				: 'max-height: 0; opacity: 0; margin-top: -10px;'}
		>
			<span class="text-muted-foreground text-xs font-medium">Run your first completion</span>
			<div
				class="border-primary/30 bg-background/50 group relative rounded-md border transition-all duration-300"
			>
				<pre class="text-foreground overflow-x-auto p-3 font-mono text-[10px] leading-relaxed sm:text-xs"><code>{codeSnippets[activeTab]}</code></pre>
				<button
					type="button"
					onclick={copyCodeSnippet}
					class="bg-primary/10 text-primary hover:bg-primary/20 focus:ring-primary absolute top-2 right-2 flex shrink-0 items-center gap-1 rounded px-1.5 py-1 text-xs font-medium transition-all duration-200 hover:scale-105 focus:outline-none focus:ring-2 focus:ring-offset-2"
					aria-label="Copy code snippet"
				>
					{#if copiedCode}
						<Check class="size-3" aria-hidden="true" />
					{:else}
						<Copy class="size-3" aria-hidden="true" />
					{/if}
				</button>
			</div>
		</div>

		<!-- CLI note -->
		<p class="text-muted-foreground text-center text-[10px]">
			For interactive development, install the <a href="https://learn.microsoft.com/en-us/azure/foundry-local/reference/reference-cli" target="_blank" rel="noopener noreferrer" class="underline hover:text-foreground">CLI</a> separately.
		</p>
	</div>
</div>
