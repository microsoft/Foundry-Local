<script lang="ts">
	import { onMount } from 'svelte';

	export let colorSrc: string;
	export let darkSrc: string;
	export let strokeSrc: string | undefined;
	export let alt = 'Foundry Local';
	export let height = 28; // height in pixels for the logo container

	let wrapperEl: HTMLSpanElement;
	let isActive = false;
	let revealStroke = colorSrc;

	$: revealStroke = strokeSrc ?? colorSrc;

	onMount(() => {
		// Play animation on mount
		isActive = true;
		const resetTimeout = setTimeout(() => {
			isActive = false;
		}, 1100); // Match the animation duration

		const activate = () => (isActive = true);
		const deactivate = () => (isActive = false);
		const targets = new Set<HTMLElement>();

		if (wrapperEl) {
			targets.add(wrapperEl);
			const parentHoverTarget = wrapperEl.closest<HTMLElement>('.logo-hover-target');
			if (parentHoverTarget) {
				targets.add(parentHoverTarget);
			}
		}

		targets.forEach((target) => {
			target.addEventListener('pointerenter', activate);
			target.addEventListener('pointerleave', deactivate);
			target.addEventListener('focusin', activate);
			target.addEventListener('focusout', deactivate);
		});

		return () => {
			clearTimeout(resetTimeout);
			targets.forEach((target) => {
				target.removeEventListener('pointerenter', activate);
				target.removeEventListener('pointerleave', deactivate);
				target.removeEventListener('focusin', activate);
				target.removeEventListener('focusout', deactivate);
			});
		};
	});
</script>

<span
	bind:this={wrapperEl}
	class={`logo-wrapper${isActive ? ' is-active' : ''}`}
	style={`height: ${height}px;`}
>
	<span class="logo-layer logo-layer--light">
		<img src={colorSrc} {alt} class="logo-base" />
		<img src={revealStroke} alt="" aria-hidden="true" class="logo-reveal logo-reveal--stroke" />
	</span>
	<span class="logo-layer logo-layer--dark">
		<img src={darkSrc} {alt} class="logo-base" />
		<img src={colorSrc} alt="" aria-hidden="true" class="logo-reveal" />
	</span>
</span>

<style>
	.logo-wrapper {
		display: inline-flex;
		align-items: center;
		position: relative;
	}

	.logo-layer {
		display: inline-flex;
		align-items: center;
		position: relative;
		height: 100%;
	}

	.logo-layer--dark {
		display: none;
	}

	.logo-base,
	.logo-reveal {
		display: block;
		height: 100%;
		width: auto;
	}

	.logo-base {
		transition: opacity 420ms ease;
	}

	.logo-reveal {
		position: absolute;
		inset: 0;
		opacity: 0;
		clip-path: polygon(0% 100%, 0% 100%, 0% 100%, 0% 100%);
		transition:
			clip-path 1100ms cubic-bezier(0.24, 0.82, 0.25, 1),
			opacity 600ms ease;
		filter: drop-shadow(0 0 0 rgba(0, 0, 0, 0));
	}

	.logo-reveal--stroke {
		mix-blend-mode: normal;
	}

	.logo-wrapper.is-active .logo-layer--light .logo-reveal {
		opacity: 1;
		clip-path: polygon(0% 100%, 0% 0%, 100% 0%, 100% 100%);
		transition:
			clip-path 820ms cubic-bezier(0.24, 0.82, 0.25, 1),
			opacity 420ms ease;
	}

	.logo-wrapper.is-active .logo-layer--light .logo-base {
		opacity: 0;
	}

	:global(.dark) .logo-layer--light {
		display: none;
	}

	:global(.dark) .logo-layer--dark {
		display: inline-flex;
	}

	:global(.dark) .logo-wrapper.is-active .logo-layer--dark .logo-reveal {
		opacity: 1;
		clip-path: polygon(0% 100%, 0% 0%, 100% 0%, 100% 100%);
		transition:
			clip-path 820ms cubic-bezier(0.24, 0.82, 0.25, 1),
			opacity 420ms ease;
	}

	:global(.dark) .logo-wrapper.is-active .logo-layer--dark .logo-base {
		opacity: 0;
	}

	@media (prefers-reduced-motion: reduce) {
		.logo-base,
		.logo-reveal {
			transition: none !important;
		}
	}
</style>
