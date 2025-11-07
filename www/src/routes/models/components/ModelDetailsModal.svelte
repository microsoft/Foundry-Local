<script lang="ts">
	import type { GroupedFoundryModel } from '../types';
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import { Calendar, Package, Copy, Check, ExternalLink } from 'lucide-svelte';
	import * as Dialog from '$lib/components/ui/dialog';
	import * as Tooltip from '$lib/components/ui/tooltip';
	import { foundryModelService } from '../service';

	export let model: GroupedFoundryModel | null;
	export let isOpen = false;
	export let copiedModelId: string | null = null;
	export let onCopyModelId: (modelId: string) => void;
	export let onCopyCommand: (modelId: string) => void;

	function getDeviceIcon(device: string): string {
		const icons: Record<string, string> = {
			npu: '🧠',
			gpu: '🎮',
			cpu: '💻'
		};
		return icons[device.toLowerCase()] || '🔧';
	}

	function getUniqueVariants(model: GroupedFoundryModel) {
		if (!model.variants || model.variants.length === 0) return [];

		const uniqueMap = new Map();
		for (const variant of model.variants) {
			if (!uniqueMap.has(variant.name)) {
				uniqueMap.set(variant.name, variant);
			}
		}
		return Array.from(uniqueMap.values());
	}

	function getVariantLabel(variant: any): string {
		const modelName = variant.name.toLowerCase();
		const device = variant.deviceSupport[0]?.toUpperCase() || '';

		if (modelName.includes('-cuda-gpu') || modelName.includes('-cuda-')) {
			if (modelName.includes('-trt-rtx-') || modelName.includes('-tensorrt-')) {
				return `${device} (CUDA + TensorRT)`;
			}
			return `${device} (CUDA)`;
		} else if (modelName.includes('-generic-gpu') || modelName.includes('webgpu')) {
			return `${device} (WebGPU)`;
		}

		if (modelName.includes('-qnn-')) {
			return `${device} (QNN)`;
		} else if (modelName.includes('-vitis-')) {
			return `${device} (Vitis)`;
		} else if (modelName.includes('-openvino-')) {
			return `${device} (OpenVINO)`;
		} else if (modelName.includes('-trt-rtx-') || modelName.includes('-tensorrt-')) {
			return `${device} (TensorRT)`;
		}

		if (modelName.includes('-generic-cpu')) {
			return `${device} (Generic)`;
		}

		return device;
	}

	function formatDate(dateString: string): string {
		return new Date(dateString).toLocaleDateString('en-US', {
			year: 'numeric',
			month: 'short',
			day: 'numeric'
		});
	}

	function formatModelCommand(modelId: string): string {
		return `foundry model run ${modelId}`;
	}

	function renderMarkdown(text: string): string {
		if (!text) return '';

		let html = text;

		// Headers
		html = html.replace(/^#### (.*?)$/gm, '<h4 class="text-base font-semibold mt-4 mb-2">$1</h4>');
		html = html.replace(/^### (.*?)$/gm, '<h3 class="text-lg font-semibold mt-5 mb-2">$1</h3>');
		html = html.replace(/^## (.*?)$/gm, '<h2 class="text-xl font-bold mt-6 mb-3">$1</h2>');
		html = html.replace(/^# (.*?)$/gm, '<h1 class="text-2xl font-bold mt-6 mb-3">$1</h1>');

		// Process lists
		const lines = html.split('\n');
		const processed: string[] = [];
		let inList = false;
		let listItems: string[] = [];

		for (let i = 0; i < lines.length; i++) {
			const line = lines[i];
			const trimmedLine = line.trim();
			const listMatch = trimmedLine.match(/^[-*]\s+(.+)$/);

			if (listMatch) {
				if (!inList) {
					inList = true;
					listItems = [];
				}
				listItems.push(listMatch[1]);
			} else {
				if (inList) {
					const listHtml =
						'<ul class="list-disc list-inside mb-3 space-y-1">' +
						listItems.map((item) => `<li>${item}</li>`).join('') +
						'</ul>';
					processed.push(listHtml);
					inList = false;
					listItems = [];
				}
				processed.push(line);
			}
		}

		if (inList) {
			const listHtml =
				'<ul class="list-disc list-inside mb-3 space-y-1">' +
				listItems.map((item) => `<li>${item}</li>`).join('') +
				'</ul>';
			processed.push(listHtml);
		}

		html = processed.join('\n');

		// Bold, italic, code, links
		html = html.replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>');
		html = html.replace(/(?<!^)(?<![-\s])\*([^\*\n]+?)\*/g, '<em>$1</em>');
		html = html.replace(/`(.*?)`/g, '<code>$1</code>');
		html = html.replace(
			/\[(.*?)\]\((.*?)\)/g,
			'<a href="$2" target="_blank" rel="noopener noreferrer" class="text-primary hover:underline">$1</a>'
		);

		// Paragraphs
		html = html
			.split('\n\n')
			.map((para) => {
				const trimmed = para.trim();
				if (
					trimmed &&
					!trimmed.startsWith('<h') &&
					!trimmed.startsWith('<ul') &&
					!trimmed.startsWith('<ol')
				) {
					return `<p class="mb-3">${para.replace(/\n/g, '<br>')}</p>`;
				}
				return para;
			})
			.join('\n');

		return html;
	}
</script>

<Dialog.Root bind:open={isOpen}>
	<Dialog.Content class="max-h-[90vh] max-w-4xl overflow-y-auto">
		{#if model}
			<Dialog.Header>
				<Dialog.Title class="flex items-center gap-3 text-2xl font-bold">
					<span>{model.displayName}</span>
					<Badge variant="secondary" class="text-xs">v{model.latestVersion}</Badge>
				</Dialog.Title>
				<Dialog.Description class="text-sm text-muted-foreground">
					{model.publisher}
				</Dialog.Description>
			</Dialog.Header>

			<div class="mt-6 space-y-6">
				<!-- Stats Section -->
				<div class="grid grid-cols-2 gap-4 sm:grid-cols-3">
					<div class="rounded-lg border bg-card p-4">
						<div class="text-2xl font-bold text-primary">{model.variants.length}</div>
						<div class="text-xs text-muted-foreground">Variants</div>
					</div>
					<div class="rounded-lg border bg-card p-4">
						<div class="text-2xl font-bold text-primary">
							{formatDate(model.lastModified)}
						</div>
						<div class="text-xs text-muted-foreground">Updated</div>
					</div>
					<div class="rounded-lg border bg-card p-4">
						<div class="flex flex-wrap gap-2">
							{#each model.deviceSupport as device}
								<div
									class="inline-flex items-center gap-1 rounded-md bg-primary/10 px-2.5 py-1 text-sm font-medium"
								>
									<span class="text-base">{getDeviceIcon(device)}</span>
									<span class="text-primary">{device.toUpperCase()}</span>
								</div>
							{/each}
						</div>
						<div class="mt-2 text-xs text-muted-foreground">Supported Devices</div>
					</div>
				</div>

				<!-- Description -->
				<div>
					<h3 class="mb-2 text-lg font-semibold">Description</h3>
					<div
						class="prose prose-sm max-w-none text-sm leading-relaxed text-muted-foreground dark:prose-invert"
					>
						{@html renderMarkdown(model.longDescription || model.description)}
					</div>
				</div>

				<!-- Model Details -->
				<div>
					<h3 class="mb-3 text-lg font-semibold">Model Information</h3>
					<div class="grid gap-3 sm:grid-cols-2">
						{#if model.taskType}
							<div class="flex items-start gap-3 rounded-lg border bg-card/50 p-3">
								<Package class="mt-0.5 size-4 text-primary" />
								<div>
									<div class="text-xs font-medium text-muted-foreground">Task Type</div>
									<div class="text-sm font-medium">{model.taskType}</div>
								</div>
							</div>
						{/if}
						{#if model.fileSizeBytes}
							<div class="flex items-start gap-3 rounded-lg border bg-card/50 p-3">
								<svg class="mt-0.5 size-4 text-primary" fill="currentColor" viewBox="0 0 20 20">
									<path
										fill-rule="evenodd"
										d="M4 4a2 2 0 012-2h4.586A2 2 0 0112 2.586L15.414 6A2 2 0 0116 7.414V16a2 2 0 01-2 2H6a2 2 0 01-2-2V4zm2 6a1 1 0 011-1h6a1 1 0 110 2H7a1 1 0 01-1-1zm1 3a1 1 0 100 2h6a1 1 0 100-2H7z"
										clip-rule="evenodd"
									/>
								</svg>
								<div>
									<div class="text-xs font-medium text-muted-foreground">File Size (max)</div>
									<div class="text-sm font-medium">{model.modelSize}</div>
								</div>
							</div>
						{/if}
						{#if model.license}
							<div class="flex items-start gap-3 rounded-lg border bg-card/50 p-3">
								<svg class="mt-0.5 size-4 text-primary" fill="currentColor" viewBox="0 0 20 20">
									<path d="M9 2a1 1 0 000 2h2a1 1 0 100-2H9z" />
									<path
										fill-rule="evenodd"
										d="M4 5a2 2 0 012-2 3 3 0 003 3h2a3 3 0 003-3 2 2 0 012 2v11a2 2 0 01-2 2H6a2 2 0 01-2-2V5zm3 4a1 1 0 000 2h.01a1 1 0 100-2H7zm3 0a1 1 0 000 2h3a1 1 0 100-2h-3zm-3 4a1 1 0 100 2h.01a1 1 0 100-2H7zm3 0a1 1 0 100 2h3a1 1 0 100-2h-3z"
										clip-rule="evenodd"
									/>
								</svg>
								<div>
									<div class="text-xs font-medium text-muted-foreground">License</div>
									<div class="text-sm font-medium">{model.license}</div>
								</div>
							</div>
						{/if}
						{#if model.acceleration}
							<div class="flex items-start gap-3 rounded-lg border bg-card/50 p-3">
								<svg class="mt-0.5 size-4 text-primary" fill="currentColor" viewBox="0 0 20 20">
									<path
										fill-rule="evenodd"
										d="M11.3 1.046A1 1 0 0112 2v5h4a1 1 0 01.82 1.573l-7 10A1 1 0 018 18v-5H4a1 1 0 01-.82-1.573l7-10a1 1 0 011.12-.38z"
										clip-rule="evenodd"
									/>
								</svg>
								<div>
									<div class="text-xs font-medium text-muted-foreground">Acceleration</div>
									<div class="text-sm font-medium">
										{foundryModelService.getAccelerationDisplayName(model.acceleration)}
									</div>
								</div>
							</div>
						{/if}
					</div>
				</div>

				<!-- Available Variants -->
				<div>
					<h3 class="mb-3 text-lg font-semibold">Available Model Variants</h3>
					<div class="space-y-3">
						{#each getUniqueVariants(model) as variant}
							<div class="rounded-lg border bg-card p-4 transition-all hover:border-primary/50">
								<div class="mb-3 flex items-start justify-between">
									<div class="flex-1">
										<div class="font-mono text-sm font-medium">{variant.name}</div>
										<div
											class="mt-1 flex flex-wrap items-center gap-2 text-xs text-muted-foreground"
										>
											<span>Device:</span>
											{#each variant.deviceSupport as device}
												<Badge variant="secondary" class="text-xs">
													{getDeviceIcon(device)}
													{getVariantLabel(variant)}
												</Badge>
											{/each}
											{#if variant.fileSizeBytes}
												<Badge variant="outline" class="text-xs">
													{foundryModelService.formatFileSize(variant.fileSizeBytes)}
												</Badge>
											{/if}
										</div>
									</div>
									<Button
										variant="outline"
										size="sm"
										onclick={(e) => {
											e.stopPropagation();
											onCopyModelId(variant.name);
										}}
										class="gap-2"
									>
										{#if copiedModelId === variant.name}
											<Check class="size-4 text-green-500" />
											Copied
										{:else}
											<Copy class="size-4" />
											Copy ID
										{/if}
									</Button>
								</div>

								<!-- Command to run -->
								<div class="rounded-md bg-muted/50 p-3">
									<div class="flex flex-col gap-2 sm:flex-row sm:items-center sm:justify-between">
										<div class="text-xs font-medium text-muted-foreground sm:w-32">
											Run Command:
										</div>
										<code class="break-all font-mono text-xs sm:flex-1">
											{formatModelCommand(variant.name)}
										</code>
										<Tooltip.Root>
											<Tooltip.Trigger>
												{#snippet child({ props })}
													<Button
														{...props}
														variant="ghost"
														size="sm"
														onclick={(e) => {
															e.stopPropagation();
															onCopyCommand(variant.name);
														}}
														class="h-6 gap-1.5 px-2 text-xs"
													>
														{#if copiedModelId === `run-${variant.name}`}
															<Check class="size-3 text-green-500" />
															Copied
														{:else}
															<Copy class="size-3" />
															Copy
														{/if}
													</Button>
												{/snippet}
											</Tooltip.Trigger>
											<Tooltip.Content side="top" align="end">
												<code class="tooltip-code">{formatModelCommand(variant.name)}</code>
											</Tooltip.Content>
										</Tooltip.Root>
									</div>
								</div>
							</div>
						{/each}
					</div>
				</div>

				<!-- Links Section -->
				{#if model.githubUrl || model.paperUrl || model.demoUrl || model.documentation}
					<div>
						<h3 class="mb-3 text-lg font-semibold">Resources</h3>
						<div class="flex flex-wrap gap-2">
							{#if model.githubUrl}
								<Button
									variant="outline"
									size="sm"
									onclick={() => model && window.open(model.githubUrl, '_blank')}
								>
									<ExternalLink class="mr-2 size-4" />
									GitHub
								</Button>
							{/if}
							{#if model.paperUrl}
								<Button
									variant="outline"
									size="sm"
									onclick={() => model && window.open(model.paperUrl, '_blank')}
								>
									<ExternalLink class="mr-2 size-4" />
									Paper
								</Button>
							{/if}
							{#if model.demoUrl}
								<Button
									variant="outline"
									size="sm"
									onclick={() => model && window.open(model.demoUrl, '_blank')}
								>
									<ExternalLink class="mr-2 size-4" />
									Demo
								</Button>
							{/if}
							{#if model.documentation}
								<Button
									variant="outline"
									size="sm"
									onclick={() => model && window.open(model.documentation, '_blank')}
								>
									<ExternalLink class="mr-2 size-4" />
									Documentation
								</Button>
							{/if}
						</div>
					</div>
				{/if}

				<!-- Tags -->
				{#if model.tags && model.tags.length > 0}
					<div>
						<h3 class="mb-2 text-lg font-semibold">Tags</h3>
						<div class="flex flex-wrap gap-2">
							{#each model.tags as tag}
								<Badge variant="outline" class="text-xs">{tag}</Badge>
							{/each}
						</div>
					</div>
				{/if}
			</div>
		{/if}
	</Dialog.Content>
</Dialog.Root>

<style>
	.prose :global(code) {
		background-color: hsl(var(--muted));
		padding: 0.125rem 0.25rem;
		border-radius: 0.25rem;
		font-size: 0.875em;
		font-family:
			ui-monospace, SFMono-Regular, 'SF Mono', Menlo, Consolas, 'Liberation Mono', monospace;
	}

	.tooltip-code {
		display: inline-block;
		font-family:
			ui-monospace, SFMono-Regular, 'SF Mono', Menlo, Consolas, 'Liberation Mono', monospace;
		white-space: nowrap;
	}

	.prose :global(a) {
		color: hsl(var(--primary));
		text-decoration: none;
	}

	.prose :global(a:hover) {
		text-decoration: underline;
	}

	.prose :global(strong) {
		font-weight: 600;
		color: hsl(var(--foreground));
	}

	.prose :global(h1),
	.prose :global(h2),
	.prose :global(h3),
	.prose :global(h4) {
		color: hsl(var(--foreground));
		line-height: 1.3;
	}

	.prose :global(h1:first-child),
	.prose :global(h2:first-child),
	.prose :global(h3:first-child),
	.prose :global(h4:first-child) {
		margin-top: 0;
	}

	.prose :global(p) {
		margin-bottom: 0.75rem;
		line-height: 1.6;
	}

	.prose :global(p:last-child) {
		margin-bottom: 0;
	}

	.prose :global(ul) {
		margin-bottom: 0.75rem;
		padding-left: 0;
	}

	.prose :global(li) {
		line-height: 1.6;
		margin-bottom: 0.25rem;
		color: hsl(var(--muted-foreground));
	}

	.prose :global(li):last-child {
		margin-bottom: 0;
	}
</style>
