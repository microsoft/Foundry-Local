<script lang="ts">
	import * as Sidebar from '$lib/components/ui/sidebar/index.js';
	import { siteConfig } from '$lib/config';
	import type { ComponentProps } from 'svelte';
	let { ref = $bindable(null), ...restProps }: ComponentProps<typeof Sidebar.Root> = $props();
	import { docsNavigation } from '$lib/components/doc-navigation.svelte';
	import { page } from '$app/state';
	import SocialMedia from './social-media.svelte';
	const path = $derived(page.url.pathname);
	const compactLogo = siteConfig.logoMark ?? siteConfig.logo;
</script>

<Sidebar.Root bind:ref {...restProps}>
	<Sidebar.Header>
		<Sidebar.Menu>
			<Sidebar.MenuItem>
				<Sidebar.MenuButton size="lg">
					{#snippet child({ props })}
						<a href="/" {...props}>
							<div
								class="flex aspect-square size-8 items-center justify-center rounded-lg bg-sidebar-primary text-sidebar-primary-foreground"
							>
								<img
									src={compactLogo}
									alt="Foundry Local logo"
									class="h-5 w-5"
								/>
							</div>
							<div class="flex flex-col gap-0.5 leading-none">
								<span class="font-semibold"> {siteConfig.title} </span>
							</div>
						</a>
					{/snippet}
				</Sidebar.MenuButton>
			</Sidebar.MenuItem>
		</Sidebar.Menu>
	</Sidebar.Header>
	<Sidebar.Content>
		<Sidebar.Group>
			<Sidebar.Menu>
				{#each docsNavigation.docNav as groupItem (groupItem.title)}
					<Sidebar.MenuItem>
						<Sidebar.MenuButton class="font-medium" isActive={path === groupItem.href}>
							{#snippet child({ props })}
								<a href={groupItem.href} {...props}>
									{groupItem.title}
								</a>
							{/snippet}
						</Sidebar.MenuButton>
						{#if groupItem.items?.length}
							<Sidebar.MenuSub>
								{#each groupItem.items as item (item.title)}
									<Sidebar.MenuSubItem>
										<Sidebar.MenuSubButton isActive={path === item.href}>
											{#snippet child({ props })}
												<a href={item.href} {...props}>{item.title}</a>
											{/snippet}
										</Sidebar.MenuSubButton>
									</Sidebar.MenuSubItem>
								{/each}
							</Sidebar.MenuSub>
						{/if}
					</Sidebar.MenuItem>
				{/each}
			</Sidebar.Menu>
		</Sidebar.Group>
	</Sidebar.Content>
	<div class="block sm:hidden">
		<Sidebar.Footer>
			<SocialMedia />
		</Sidebar.Footer>
	</div>

	<Sidebar.Rail />
</Sidebar.Root>
