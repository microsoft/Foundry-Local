<script lang="ts">
	import { features } from '$lib/config';
	import { Button } from '$lib/components/ui/button';
	import { ArrowRight } from 'lucide-svelte';
</script>

<div class="bg-white dark:bg-neutral-950">
	<div class="mx-auto max-w-[85rem] px-4 py-16 sm:px-6 lg:px-8 lg:py-20">
		<!-- Section Header -->
		<div class="mx-auto mb-16 max-w-3xl text-center">
			<h2 class="mb-4 text-3xl font-bold tracking-tight text-gray-900 dark:text-white sm:text-4xl">
				Run Microsoft AI locally with complete control
			</h2>
			<p class="text-lg text-gray-600 dark:text-neutral-400">
				Foundry Local brings the power of Azure AI to your environment, with flexible deployment
				options and enterprise-grade security.
			</p>
		</div>

		<!-- Features Grid -->
		<div class="grid gap-8 sm:grid-cols-2 lg:grid-cols-4">
			{#each features as feature}
				<div
					class="group relative overflow-hidden rounded-lg border border-gray-200 bg-white shadow-sm transition hover:shadow-md dark:border-neutral-800 dark:bg-neutral-900/50"
				>
					<div class="p-6">
						<div
							class="mb-4 flex size-12 items-center justify-center rounded-full bg-primary/10 dark:bg-primary/20"
						>
							<feature.icon class="size-6 shrink-0 text-primary" />
						</div>
						<div>
							<h3 class="mb-2 text-xl font-semibold text-gray-900 dark:text-white">
								{feature.title}
							</h3>
							<p class="text-gray-600 dark:text-neutral-400">
								{feature.description}
							</p>
						</div>
					</div>
				</div>
			{/each}
		</div>

		<!-- Extended Section -->
		<div class="mt-20">
			<div class="grid gap-10 lg:grid-cols-2">
				<!-- Data Privacy Section -->
				<div
					class="overflow-hidden rounded-xl bg-gradient-to-br from-blue-50 to-blue-100 dark:from-blue-950/40 dark:to-indigo-950/40"
				>
					<div class="p-8">
						<div class="mb-6 flex size-14 items-center justify-center rounded-full bg-primary/20">
							<svg
								class="size-7 text-primary"
								xmlns="http://www.w3.org/2000/svg"
								viewBox="0 0 24 24"
								fill="none"
								stroke="currentColor"
								stroke-width="2"
								stroke-linecap="round"
								stroke-linejoin="round"
							>
								<rect width="18" height="18" x="3" y="3" rx="2" ry="2" />
								<path d="M8 7V7" />
								<path d="M12 7V7" />
								<path d="M16 7V7" />
								<path d="M8 12V12" />
								<path d="M12 12V12" />
								<path d="M16 12V12" />
								<path d="M8 17V17" />
								<path d="M12 17V17" />
								<path d="M16 17V17" />
							</svg>
						</div>
						<h3 class="mb-3 text-2xl font-bold text-gray-900 dark:text-white">
							Complete Data Privacy
						</h3>
						<p class="mb-5 text-gray-700 dark:text-neutral-300">
							All data processing happens locally on your hardware. No data is sent to Microsoft or
							any external services, making Foundry Local ideal for sensitive workloads and private
							environments.
						</p>
						<ul class="mb-6 space-y-2 text-gray-700 dark:text-neutral-300">
							<li class="flex items-center">
								<svg
									class="mr-2 size-4 text-primary"
									viewBox="0 0 24 24"
									fill="none"
									xmlns="http://www.w3.org/2000/svg"
								>
									<path
										d="M20 6L9 17L4 12"
										stroke="currentColor"
										stroke-width="2"
										stroke-linecap="round"
										stroke-linejoin="round"
									/>
								</svg>
								All processing stays on your device
							</li>
							<li class="flex items-center">
								<svg
									class="mr-2 size-4 text-primary"
									viewBox="0 0 24 24"
									fill="none"
									xmlns="http://www.w3.org/2000/svg"
								>
									<path
										d="M20 6L9 17L4 12"
										stroke="currentColor"
										stroke-width="2"
										stroke-linecap="round"
										stroke-linejoin="round"
									/>
								</svg>
								Works in air-gapped environments
							</li>
						</ul>
						<Button
							variant="outline"
							href="https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started"
							target="_blank"
							rel="noopener noreferrer"
						>
							Learn about privacy features
							<ArrowRight class="ml-1 size-4" />
						</Button>
					</div>
				</div>

				<!-- Azure Compatibility Section -->
				<div
					class="overflow-hidden rounded-xl bg-gradient-to-br from-indigo-50 to-purple-100 dark:from-indigo-950/40 dark:to-purple-950/40"
				>
					<div class="p-8">
						<div
							class="mb-6 flex size-14 items-center justify-center rounded-full bg-indigo-500/20"
						>
							<svg
								class="size-7 text-indigo-600 dark:text-indigo-400"
								xmlns="http://www.w3.org/2000/svg"
								viewBox="0 0 24 24"
								fill="none"
								stroke="currentColor"
								stroke-width="2"
								stroke-linecap="round"
								stroke-linejoin="round"
							>
								<path
									d="M7 16a4 4 0 0 1-.88-7.903A5 5 0 1 1 15.9 6L16 6a5 5 0 0 1 1 9.9M15 13l-3-3m0 0-3 3m3-3v12"
								/>
							</svg>
						</div>
						<h3 class="mb-3 text-2xl font-bold text-gray-900 dark:text-white">
							OpenAI-Compatible API
						</h3>
						<p class="mb-5 text-gray-700 dark:text-neutral-300">
							Foundry Local provides an OpenAI-compatible REST API, making it easy to integrate with
							existing applications and development tools.
						</p>
						<ul class="mb-6 space-y-2 text-gray-700 dark:text-neutral-300">
							<li class="flex items-center">
								<svg
									class="mr-2 size-4 text-indigo-600 dark:text-indigo-400"
									viewBox="0 0 24 24"
									fill="none"
									xmlns="http://www.w3.org/2000/svg"
								>
									<path
										d="M20 6L9 17L4 12"
										stroke="currentColor"
										stroke-width="2"
										stroke-linecap="round"
										stroke-linejoin="round"
									/>
								</svg>
								OpenAI-compatible REST API
							</li>
							<li class="flex items-center">
								<svg
									class="mr-2 size-4 text-indigo-600 dark:text-indigo-400"
									viewBox="0 0 24 24"
									fill="none"
									xmlns="http://www.w3.org/2000/svg"
								>
									<path
										d="M20 6L9 17L4 12"
										stroke="currentColor"
										stroke-width="2"
										stroke-linecap="round"
										stroke-linejoin="round"
									/>
								</svg>
								Python and JavaScript SDKs
							</li>
							<li class="flex items-center">
								<svg
									class="mr-2 size-4 text-indigo-600 dark:text-indigo-400"
									viewBox="0 0 24 24"
									fill="none"
									xmlns="http://www.w3.org/2000/svg"
								>
									<path
										d="M20 6L9 17L4 12"
										stroke="currentColor"
										stroke-width="2"
										stroke-linecap="round"
										stroke-linejoin="round"
									/>
								</svg>
								Compatible with popular AI frameworks
							</li>
						</ul>
						<Button
							variant="outline"
							href="https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started"
							target="_blank"
							rel="noopener noreferrer"
						>
							Explore API documentation
							<ArrowRight class="ml-1 size-4" />
						</Button>
					</div>
				</div>
			</div>
		</div>
	</div>
</div>
