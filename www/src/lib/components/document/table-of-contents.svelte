<script lang="ts">
	import Button from '../ui/button/button.svelte';
	import { TableOfContents } from './toc.svelte.js';
	import { ChevronRight } from 'lucide-svelte';
	let toc = $state(TableOfContents.getInstance());
</script>

<div>
	<h5
		id="docs-on-this-page-sidebar-label"
		class="mb-3 text-sm font-semibold text-gray-800 dark:text-neutral-300"
	>
		On this page
	</h5>
	<ul>
		{#each toc.headings as heading}
			<li>
				<Button
					variant="link"
					href="#{heading.id}"
					style="margin-left: {(heading.level - 1) * 1}rem"
					class="font-base text-sm {toc.activeId === heading.id
						? 'font-medium text-primary'
						: 'text-gray-800 dark:text-neutral-300'}"
				>
					{#if heading.level > 1}
						<ChevronRight class="mt-[5px] size-3.5 shrink-0" />
					{/if}
					{heading.text}
				</Button>
			</li>
		{/each}
	</ul>
</div>
