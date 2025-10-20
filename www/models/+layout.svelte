<script lang="ts">
	import Nav from '$lib/components/home/nav.svelte';
	import Footer from '$lib/components/home/footer.svelte';
	import { page } from '$app/stores';
</script>

<Nav />
<main id="main-content">
	<slot />
</main>
<Footer />
