<script lang="ts">
	let { class: className = '' }: { class?: string } = $props();
</script>

<div class="bg-muted animate-pulse rounded-md {className}"></div>

<style>
	@keyframes pulse {
		0%,
		100% {
			opacity: 1;
		}
		50% {
			opacity: 0.5;
		}
	}

	.animate-pulse {
		animation: pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
	}
</style>
