<script>
	import { navItems, siteConfig } from '$lib/config';
	import SocialMedia from '../social-media.svelte';
	import { animate } from '$lib/utils/animations';
	import LogoTransition from '$lib/components/logo-transition.svelte';
</script>

<footer
	use:animate={{ delay: 0, duration: 800, animation: 'fade-in', threshold: 0.1 }}
	class="mx-auto mt-auto w-full max-w-[85rem] px-4 py-10 sm:px-6 lg:px-8"
>
	<div class="grid grid-cols-1 gap-8 sm:grid-cols-2 md:grid-cols-4">
		<!-- Logo and description -->
		<div class="col-span-1 md:col-span-1">
			<a href="/" class="logo-hover-target inline-flex">
				<LogoTransition
					colorSrc={siteConfig.logo}
					darkSrc={siteConfig.logoDark}
					strokeSrc={siteConfig.logoMark}
					height={32}
				/>
			</a>
			<p class="mt-3 text-sm text-gray-600 dark:text-neutral-400">
				Run AI models locally with the power of Microsoft's Azure AI technology.
			</p>
		</div>

		<!-- Product links -->
		<div>
			<h3 class="mb-3 text-sm font-semibold uppercase text-gray-900 dark:text-white">Product</h3>
			<ul class="space-y-2 text-sm">
				<li>
					<a
						href="https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started"
						target="_blank"
						rel="noopener noreferrer"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>Documentation</a
					>
				</li>
				<li>
					<a
						href="/models"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>Model Hub</a
					>
				</li>
				<li>
					<a
						href="https://github.com/microsoft/foundry-local"
						target="_blank"
						rel="noopener noreferrer"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>GitHub Repository</a
					>
				</li>
				<li>
					<a
						href="https://github.com/microsoft/foundry-local/releases"
						target="_blank"
						rel="noopener noreferrer"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>Releases</a
					>
				</li>
			</ul>
		</div>

		<!-- Resources links -->
		<div>
			<h3 class="mb-3 text-sm font-semibold uppercase text-gray-900 dark:text-white">Resources</h3>
			<ul class="space-y-2 text-sm">
				<li>
					<a
						href="https://github.com/microsoft/foundry-local/discussions"
						target="_blank"
						rel="noopener noreferrer"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>Community</a
					>
				</li>
				<li>
					<a
						href="https://azure.microsoft.com/en-us/products/ai-studio"
						target="_blank"
						rel="noopener noreferrer"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>Azure AI Foundry</a
					>
				</li>
				<li>
					<a
						href="https://github.com/microsoft/Foundry-Local/tree/main/samples"
						target="_blank"
						rel="noopener noreferrer"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>Examples</a
					>
				</li>
				<li>
					<a
						href="https://github.com/microsoft/foundry-local/issues"
						target="_blank"
						rel="noopener noreferrer"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>Support</a
					>
				</li>
			</ul>
		</div>

		<!-- Legal links -->
		<div>
			<h3 class="mb-3 text-sm font-semibold uppercase text-gray-900 dark:text-white">Legal</h3>
			<ul class="space-y-2 text-sm">
				<li>
					<a
						href="https://privacy.microsoft.com/en-us/privacystatement"
						target="_blank"
						rel="noopener noreferrer"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>Privacy Policy</a
					>
				</li>
				<li>
					<a
						href="https://www.microsoft.com/en-us/legal/terms-of-use"
						target="_blank"
						rel="noopener noreferrer"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>Terms of Use</a
					>
				</li>
				<li>
					<a
						href="https://www.microsoft.com/en-us/licensing/product-licensing/products"
						target="_blank"
						rel="noopener noreferrer"
						class="text-gray-600 hover:text-primary dark:text-neutral-400 dark:hover:text-primary"
						>Licensing</a
					>
				</li>
			</ul>
		</div>
	</div>

	<!-- Copyright and social -->
	<div class="mt-8 border-t border-gray-200 pt-8 dark:border-neutral-700">
		<div class="flex flex-col items-center gap-4 sm:flex-row sm:justify-between">
			<p class="text-sm text-gray-600 dark:text-neutral-400">
				&copy; 2025 Microsoft Corporation. All rights reserved.
			</p>
			<SocialMedia />
		</div>
	</div>
</footer>
