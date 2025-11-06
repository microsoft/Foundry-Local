<script lang="ts">
	import Skeleton from '$lib/components/ui/skeleton.svelte';
	import Card from '$lib/components/ui/card/card.svelte';
</script>

<Card class="border-border group relative overflow-hidden border transition-all duration-300">
	<div class="p-6">
		<!-- Header -->
		<div class="mb-4 flex items-start justify-between">
			<div class="flex-1">
				<Skeleton class="mb-2 h-6 w-3/4" />
				<Skeleton class="h-4 w-1/2" />
			</div>
		</div>

		<!-- Description -->
		<Skeleton class="mb-4 h-4 w-full" />
		<Skeleton class="mb-4 h-4 w-5/6" />

		<!-- Tags -->
		<div class="mb-4 flex flex-wrap gap-2">
			<Skeleton class="h-6 w-16" />
			<Skeleton class="h-6 w-20" />
			<Skeleton class="h-6 w-14" />
		</div>

		<!-- Device badges -->
		<div class="flex flex-wrap gap-2">
			<Skeleton class="h-8 w-20" />
			<Skeleton class="h-8 w-20" />
		</div>
	</div>
</Card>
