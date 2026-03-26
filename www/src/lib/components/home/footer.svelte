<script>
	import { navItems, siteConfig } from '$lib/config';
	import SocialMedia from '../social-media.svelte';
	import { animate } from '$lib/utils/animations';
	import LogoTransition from '$lib/components/logo-transition.svelte';
</script>

<footer
	use:animate={{ delay: 0, duration: 800, animation: 'fade-in', threshold: 0.1 }}
	class="border-t border-border bg-muted/30"
>
	<div class="mx-auto w-full max-w-[85rem] px-4 py-12 sm:px-6 lg:px-8">
		<div class="grid grid-cols-1 gap-8 sm:grid-cols-2 lg:grid-cols-12">
			<!-- Logo and description -->
			<div class="sm:col-span-2 lg:col-span-6">
				<a href="/" class="logo-hover-target mb-4 inline-flex">
					<LogoTransition
						colorSrc={siteConfig.logo}
						darkSrc={siteConfig.logoDark}
						strokeSrc={siteConfig.logoMark}
						height={32}
						alt="Foundry Local"
					/>
				</a>
				<p class="max-w-xs text-sm text-muted-foreground">
					Run AI models locally with the power of Microsoft's Azure AI technology.
				</p>
				<div class="mt-6">
					<SocialMedia />
				</div>
			</div>

			<!-- Links - Product & Resources -->
			<nav class="lg:col-span-3" aria-label="Product links">
				<h3 class="mb-4 text-sm font-semibold text-foreground">Product</h3>
				<ul class="space-y-3 text-sm">
					<li>
						<a
							href="https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started"
							target="_blank"
							rel="noopener noreferrer"
							class="text-muted-foreground transition-colors hover:text-foreground"
							aria-label="Documentation (opens in new tab)">Documentation</a
						>
					</li>
					<li>
						<a href="/models" class="text-muted-foreground transition-colors hover:text-foreground"
							>Model Hub</a
						>
					</li>
					<li>
						<a
							href="https://github.com/microsoft/Foundry-Local/tree/main/samples"
							target="_blank"
							rel="noopener noreferrer"
							class="text-muted-foreground transition-colors hover:text-foreground"
							aria-label="Examples (opens in new tab)">Examples</a
						>
					</li>
					<li>
						<a
							href="https://github.com/microsoft/foundry-local/releases"
							target="_blank"
							rel="noopener noreferrer"
							class="text-muted-foreground transition-colors hover:text-foreground"
							aria-label="Releases (opens in new tab)">Releases</a
						>
					</li>
				</ul>
			</nav>

			<!-- Community links -->
			<nav class="lg:col-span-3" aria-label="Community links">
				<h3 class="mb-4 text-sm font-semibold text-foreground">Community</h3>
				<ul class="space-y-3 text-sm">
					<li>
						<a
							href="https://github.com/microsoft/foundry-local"
							target="_blank"
							rel="noopener noreferrer"
							class="text-muted-foreground transition-colors hover:text-foreground"
							aria-label="GitHub (opens in new tab)">GitHub</a
						>
					</li>
					<li>
						<a
							href="https://github.com/microsoft/foundry-local/issues"
							target="_blank"
							rel="noopener noreferrer"
							class="text-muted-foreground transition-colors hover:text-foreground"
							aria-label="Support (opens in new tab)">Support</a
						>
					</li>
					<li>
						<a
							href="https://github.com/microsoft/Foundry-Local/blob/main/LICENSE"
							target="_blank"
							rel="noopener noreferrer"
							class="text-muted-foreground transition-colors hover:text-foreground"
							aria-label="MIT License (opens in new tab)">MIT License</a
						>
					</li>
				</ul>
			</nav>
		</div>

		<!-- Copyright -->
		<div class="mt-12 border-t border-border pt-8">
			<div class="flex flex-col items-center gap-2">
				<p class="text-center text-sm text-muted-foreground">
					&copy; {new Date().getFullYear()} Microsoft Corporation. All rights reserved.
				</p>
				<a
					href="https://www.microsoft.com/en-us/privacy/privacystatement"
					target="_blank"
					rel="noopener noreferrer"
					class="text-sm text-muted-foreground transition-colors hover:text-foreground"
					aria-label="Microsoft Privacy Statement (opens in new tab)"
				>
					Privacy Statement
				</a>
			</div>
		</div>
	</div>
</footer>
