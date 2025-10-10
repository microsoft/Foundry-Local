<script lang="ts">
	import { Button, buttonVariants } from '$lib/components/ui/button';
	import { Separator } from '$lib/components/ui/separator';
	import { ChevronDown, Menu, Download, BookOpen, Server } from 'lucide-svelte';
	import * as DropdownMenu from '$lib/components/ui/dropdown-menu';
	import DarkModeToggle from '../dark-mode-toggle.svelte';
	import { navItems, siteConfig } from '$lib/config';
	import SocialMedia from '../social-media.svelte';
	let isNavOpen = false;
</script>

<a
	href="#main-content"
	class="sr-only focus:not-sr-only focus:absolute focus:left-4 focus:top-4 focus:z-50 focus:rounded focus:bg-primary focus:px-4 focus:py-2 focus:text-white focus:shadow-lg"
>
	Skip to main content
</a>
<header class="sticky top-0 z-50 w-full border-b bg-white dark:bg-black">
	<nav
		class="relative mx-auto w-full max-w-[85rem] px-4 py-3 sm:px-6 md:flex md:items-center md:justify-between md:gap-3 lg:px-8"
		aria-label="Main navigation"
	>
		<!-- Logo w/ Collapse Button -->
		<div class="flex items-center justify-between">
			<a href="/" class="flex items-center gap-2">
				<img src={siteConfig.logo} alt="Foundry Local" class="h-7 dark:hidden" />
				<img src={siteConfig.logoDark} alt="Foundry Local" class="hidden h-7 dark:block" />
				<span class="ml-1 whitespace-nowrap text-lg font-semibold">Foundry Local</span>
			</a>
			<div class="flex items-center gap-2">
				<Button
					variant="outline"
					size="icon"
					class="md:hidden"
					onclick={() => (isNavOpen = !isNavOpen)}
					aria-label="Toggle navigation menu"
					aria-expanded={isNavOpen}
				>
					<Menu />
					<span class="sr-only">Toggle navigation</span>
				</Button>
				<div class="md:hidden">
					<DarkModeToggle />
				</div>
			</div>
		</div>

		<!-- Navigation Menu -->
		<div
			class={`${
				isNavOpen ? 'block' : 'hidden'
			} grow basis-full overflow-hidden transition-all duration-300 md:block`}
			aria-labelledby="header-collapse"
		>
			<div
				class="max-h-[75vh] overflow-hidden overflow-y-auto [&::-webkit-scrollbar-thumb]:rounded-full [&::-webkit-scrollbar-thumb]:bg-gray-300 dark:[&::-webkit-scrollbar-thumb]:bg-neutral-500 [&::-webkit-scrollbar-track]:bg-gray-100 dark:[&::-webkit-scrollbar-track]:bg-neutral-700 [&::-webkit-scrollbar]:w-2"
			>
				<div
					class="flex flex-col gap-0.5 py-2 md:flex-row md:items-center md:justify-end md:gap-1 md:py-0"
				>
					<!-- Standard Navigation Items -->
					{#each navItems as item}
						{#if item.items && item.items?.length > 0}
							<DropdownMenu.Root>
								<DropdownMenu.Trigger class="{buttonVariants({ variant: 'ghost' })} ">
									{#if item.icon}<item.icon class="mr-1 size-4" />{/if}
									{item.title}
									<ChevronDown class="ml-1 size-4" />
								</DropdownMenu.Trigger>
								<DropdownMenu.Content>
									<DropdownMenu.Group>
										{#each item.items as subItem}
											<DropdownMenu.Item>
												<a 
													href={subItem.href}
													target={subItem.href?.startsWith('http') ? '_blank' : undefined}
													rel={subItem.href?.startsWith('http') ? 'noopener noreferrer' : undefined}
												>{subItem.title}</a>
											</DropdownMenu.Item>
										{/each}
									</DropdownMenu.Group>
								</DropdownMenu.Content>
							</DropdownMenu.Root>
						{:else}
							<Button 
								variant="ghost" 
								href={item.href}
								target={item.href?.startsWith('http') ? '_blank' : undefined}
								rel={item.href?.startsWith('http') ? 'noopener noreferrer' : undefined}
							>
								{#if item.icon}<item.icon class="mr-1 size-4" />{/if}
								{item.title}
							</Button>
						{/if}
					{/each}

					<SocialMedia />
					<Separator orientation="vertical" class="mr-2 hidden h-5 md:block" />

					<div class="hidden md:block">
						<DarkModeToggle />
					</div>
				</div>
			</div>
		</div>
	</nav>
</header>
