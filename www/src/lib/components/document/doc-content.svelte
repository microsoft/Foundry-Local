<script lang="ts">
	import { onMount } from 'svelte';
	import { mount } from 'svelte';
	import { TableOfContents } from './toc.svelte.js';
	import { animate } from '$lib/utils/animations';
	import CodeCopyButton from '../code-copy-button.svelte';
	import { Link } from 'lucide-svelte';

	let contentRef: HTMLElement | undefined = $state();
	let { highlighter, theme, data }: { highlighter: any; theme: string | undefined; data: any } =
		$props();

	let toc = $state(TableOfContents.getInstance());

	function addAnchorLinks() {
		if (!contentRef) return;

		const headings = contentRef.querySelectorAll('h2, h3, h4, h5, h6');
		headings.forEach((heading) => {
			const id = heading.id;
			if (!id) return;

			// Check if anchor link already exists
			if (heading.querySelector('.heading-anchor')) return;

			// Create anchor link
			const anchor = document.createElement('a');
			anchor.href = `#${id}`;
			anchor.className =
				'heading-anchor ml-2 text-muted-foreground opacity-0 transition-opacity hover:opacity-100 inline-flex items-center';
			anchor.setAttribute('aria-label', `Link to ${heading.textContent}`);
			anchor.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"/><path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"/></svg>`;

			heading.appendChild(anchor);

			// Show anchor on heading hover
			heading.classList.add('group');
			anchor.classList.add('group-hover:opacity-100');
		});
	}

	async function highlightCode() {
		if (!contentRef || !highlighter) return;

		const codeBlocks = contentRef.querySelectorAll('pre code');
		for (const block of codeBlocks) {
			// Skip if already highlighted
			if (block.closest('pre')?.classList.contains('shiki')) continue;

			const code = block.textContent || '';
			const language = block.getAttribute('class')?.replace('language-', '') || 'text';

			const highlightedCode = await highlighter.codeToHtml(code, {
				lang: language,
				theme: theme === 'dark' ? 'github-dark' : 'github-light'
			});

			const wrapper = block.closest('pre');
			if (wrapper) {
				// Extract the inner content of the highlighted code (inside the pre tag)
				const tempDiv = document.createElement('div');
				tempDiv.innerHTML = highlightedCode;
				const innerContent = tempDiv.querySelector('.shiki')?.innerHTML || '';

				// Keep the original pre tag but update its classes and content
				wrapper.className = 'shiki shiki-wrapper not-prose relative group';
				wrapper.style.backgroundColor = theme === 'github-dark' ? '#0d1117' : '#f6f8fa';
				wrapper.innerHTML = innerContent;

				// Mount copy button
				const buttonContainer = document.createElement('div');
				buttonContainer.className = 'absolute top-2 right-2';
				wrapper.appendChild(buttonContainer);

				mount(CodeCopyButton, {
					target: buttonContainer,
					props: { code }
				});
			}
		}
	}

	onMount(() => {
		if (contentRef) {
			toc.updateContentRef(contentRef);
			// Small delay to ensure content is fully rendered
			setTimeout(() => {
				highlightCode();
				addAnchorLinks();
			}, 0);
		}
	});
</script>

<div
	use:animate={{ delay: 100, duration: 600, animation: 'fade-in' }}
	class="prose prose-slate max-w-none dark:prose-invert"
	bind:this={contentRef}
>
	<data.doc />
</div>

<style>
	:global(.prose h2),
	:global(.prose h3),
	:global(.prose h4),
	:global(.prose h5),
	:global(.prose h6) {
		position: relative;
	}
</style>
