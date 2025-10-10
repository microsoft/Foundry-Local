<script lang="ts">
	import * as Dialog from '$lib/components/ui/dialog/index.js';
	import { TableOfContents } from './toc.svelte.js';
	import Button, { buttonVariants } from '../ui/button/button.svelte';
	import { AlignLeft, ChevronRight } from 'lucide-svelte';
	import { ScrollArea } from '$lib/components/ui/scroll-area/index.js';

	let toc = $state(TableOfContents.getInstance());
	let opened = $state(false);
</script>

<Dialog.Root bind:open={opened}>
	<Dialog.Trigger class="fixed bottom-2 right-2 {buttonVariants({ variant: 'default' })}">
		<AlignLeft />
		On This Page
	</Dialog.Trigger>
	<Dialog.Content>
		<Dialog.Header>
			<Dialog.Title>On this page</Dialog.Title>
		</Dialog.Header>
		<ScrollArea class="h-80">
			<ul>
				{#each toc.headings as heading}
					<li>
						<Button
							variant="link"
							href="#{heading.id}"
							style="margin-left: {(heading.level - 1) * 1}rem"
							class="font-base text-sm text-gray-800 dark:text-neutral-300"
							onclick={() => (opened = false)}
						>
							{#if heading.level > 1}
								<ChevronRight class="mt-[5px] size-3.5 shrink-0" />
							{/if}
							{heading.text}
						</Button>
					</li>
				{/each}
			</ul>
		</ScrollArea>
	</Dialog.Content>
</Dialog.Root>
