<script lang="ts">
	import type { GroupedFoundryModel } from '../types';
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import { Calendar, Copy, Check } from 'lucide-svelte';
	import * as Card from '$lib/components/ui/card';
	import * as Tooltip from '$lib/components/ui/tooltip';
	import { animate } from '$lib/utils/animations';

	export let model: GroupedFoundryModel;
	export let copiedModelId: string | null = null;
	export let onCardClick: (model: GroupedFoundryModel) => void;
	export let onCopyCommand: (modelId: string) => void;

	function getDeviceIcon(device: string): string {
		const icons: Record<string, string> = {
			npu: '🧠',
			gpu: '🎮',
			cpu: '💻'
		};
		return icons[device.toLowerCase()] || '🔧';
	}

	function getUniqueVariants() {
		if (!model.variants || model.variants.length === 0) return [];

		const uniqueMap = new Map();
		for (const variant of model.variants) {
			if (!uniqueMap.has(variant.name)) {
				uniqueMap.set(variant.name, variant);
			}
		}
		return Array.from(uniqueMap.values());
	}

	function sortVariantsByDevice(variants: any[]) {
		const devicePriority: Record<string, number> = {
			cpu: 1,
			gpu: 2,
			npu: 3
		};

		return [...variants].sort((a, b) => {
			const aDevice = a.deviceSupport[0]?.toLowerCase() || 'zzz';
			const bDevice = b.deviceSupport[0]?.toLowerCase() || 'zzz';

			const aPriority = devicePriority[aDevice] || 99;
			const bPriority = devicePriority[bDevice] || 99;

			return aPriority - bPriority;
		});
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

	function formatModelCommand(modelId: string): string {
		return `foundry model run ${modelId}`;
	}

	function cleanDescription(description: string): string {
		// Find the first occurrence of markdown heading (starting with #)
		const markdownIndex = description.indexOf('#');
		if (markdownIndex > 0) {
			// Return text before the markdown, trimmed
			return description.substring(0, markdownIndex).trim();
		}
		return description;
	}

	$: uniqueVariants = sortVariantsByDevice(getUniqueVariants());
	$: displayDescription = cleanDescription(model.description);
</script>

<div use:animate={{ delay: 0, duration: 600, animation: 'fade-in', once: true }} class="flex">
	<Card.Root
		class="hover:border-primary/50 flex flex-1 cursor-pointer flex-col transition-all duration-300 hover:-translate-y-1 hover:shadow-xl"
		onclick={() => onCardClick(model)}
	>
		<Card.Header class="pb-3">
			<Card.Title class="line-clamp-1 text-lg">
				{model.displayName}
			</Card.Title>
			<Card.Description class="text-xs">{model.publisher}</Card.Description>
		</Card.Header>
		<Card.Content class="flex flex-1 flex-col pt-0">
			<p class="mb-3 text-sm text-gray-600 dark:text-gray-400">
				{displayDescription}
			</p>

			<!-- Badges -->
			<div class="mb-3 mt-auto flex flex-row flex-wrap items-center gap-1.5">
				<div class="flex shrink-0 items-center gap-1 text-xs text-gray-500">
					<Calendar class="size-3" />
					<span class="whitespace-nowrap"
						>{new Date(model.lastModified).toLocaleDateString('en-US', {
							month: 'short',
							year: 'numeric'
						})}</span
					>
				</div>
				<Badge variant="secondary" class="shrink-0 text-xs">v{model.latestVersion}</Badge>
				{#if model.taskType}
					<Badge variant="secondary" class="shrink-0 text-xs">{model.taskType}</Badge>
				{/if}
				{#if model.license}
					<Badge variant="outline" class="flex shrink-0 items-center gap-1 text-xs">
						<svg class="size-3" fill="currentColor" viewBox="0 0 20 20">
							<path d="M9 2a1 1 0 000 2h2a1 1 0 100-2H9z" />
							<path
								fill-rule="evenodd"
								d="M4 5a2 2 0 012-2 3 3 0 003 3h2a3 3 0 003-3 2 2 0 012 2v11a2 2 0 01-2 2H6a2 2 0 01-2-2V5zm3 4a1 1 0 000 2h.01a1 1 0 100-2H7zm3 0a1 1 0 000 2h3a1 1 0 100-2h-3zm-3 4a1 1 0 100 2h.01a1 1 0 100-2H7zm3 0a1 1 0 100 2h3a1 1 0 100-2h-3z"
								clip-rule="evenodd"
							/>
						</svg>
						{model.license}
					</Badge>
				{/if}
			</div>

			<!-- Copy Run Command Section -->
			<div class="border-t pt-3">
				{#if uniqueVariants.length > 0}
					<div class="space-y-1.5">
						<div class="text-xs font-medium text-gray-600 dark:text-gray-400">
							Copy Run Command:
						</div>
						<div class="grid grid-cols-2 gap-1.5">
							{#each uniqueVariants as variant (variant.name)}
								<Tooltip.Root>
									<Tooltip.Trigger>
										{#snippet child({ props })}
											<Button
												{...props}
												variant="outline"
												size="sm"
												onclick={(e) => {
													e.stopPropagation();
													onCopyCommand(variant.name);
												}}
												class="h-7 w-full justify-start gap-1.5 px-2.5 text-xs"
											>
												{#if copiedModelId === `run-${variant.name}`}
													<Check class="size-3 shrink-0 text-green-500" />
													<span class="min-w-0 truncate">Copied!</span>
												{:else}
													<span class="shrink-0"
														>{getDeviceIcon(variant.deviceSupport[0] || '')}</span
													>
													<span class="min-w-0 truncate">{getVariantLabel(variant)}</span>
												{/if}
											</Button>
										{/snippet}
									</Tooltip.Trigger>
									<Tooltip.Content side="top" align="center">
										<code class="tooltip-code">{formatModelCommand(variant.name)}</code>
									</Tooltip.Content>
								</Tooltip.Root>
							{/each}
						</div>
					</div>
				{/if}
			</div>
		</Card.Content>
	</Card.Root>
</div>

<style>
	:global(.line-clamp-1) {
		display: -webkit-box;
		-webkit-line-clamp: 1;
		line-clamp: 1;
		-webkit-box-orient: vertical;
		overflow: hidden;
	}

	:global(.line-clamp-2) {
		display: -webkit-box;
		-webkit-line-clamp: 2;
		line-clamp: 2;
		-webkit-box-orient: vertical;
		overflow: hidden;
	}

	:global(.line-clamp-3) {
		display: -webkit-box;
		-webkit-line-clamp: 3;
		line-clamp: 3;
		-webkit-box-orient: vertical;
		overflow: hidden;
	}

	.tooltip-code {
		display: inline-block;
		font-family:
			ui-monospace, SFMono-Regular, 'SF Mono', Menlo, Consolas, 'Liberation Mono', monospace;
		white-space: nowrap;
	}
</style>
