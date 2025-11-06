<script lang="ts">
	import AppSidebar from '$lib/components/app-sidebar.svelte';
	import Breadcrumb from '$lib/components/breadcrumb.svelte';
	import DarkModeToggle from '$lib/components/dark-mode-toggle.svelte';
	import SearchBar from '$lib/components/search-bar.svelte';
	import SocialMedia from '$lib/components/social-media.svelte';
	import BackToTop from '$lib/components/back-to-top.svelte';
	import { Separator } from '$lib/components/ui/separator/index.js';
	import * as Sidebar from '$lib/components/ui/sidebar/index.js';
	let { children }: { children: any } = $props();
</script>

<Sidebar.Provider>
	<AppSidebar />
	<Sidebar.Inset>
		<header
			class="sticky top-0 z-10 flex shrink-0 items-center justify-between gap-2 border-b bg-background/80 px-4 backdrop-blur-md transition-all duration-300"
		>
			<div class="flex h-16 items-center gap-2">
				<Sidebar.Trigger class="-ml-1" aria-label="Toggle sidebar" />
				<Separator orientation="vertical" class="mr-2 h-4" />
				<Breadcrumb />
			</div>

			<div class="flex items-center gap-2">
				<SearchBar />
				<div class="hidden sm:block">
					<SocialMedia />
				</div>
				<DarkModeToggle />
			</div>
		</header>
		<div class="flex flex-col gap-4 p-4">
			{@render children()}
		</div>
	</Sidebar.Inset>
	<BackToTop />
</Sidebar.Provider>
