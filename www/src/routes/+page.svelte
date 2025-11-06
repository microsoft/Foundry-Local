<script>
	import Features from '$lib/components/home/features.svelte';
	import Footer from '$lib/components/home/footer.svelte';
	import Hero from '$lib/components/home/hero.svelte';
	import Nav from '$lib/components/home/nav.svelte';
	import BackToTop from '$lib/components/back-to-top.svelte';

	let isDownloadOpen = $state(false);
</script>

<Nav bind:isDownloadOpen />
<main id="main-content">
	<Hero bind:isDownloadOpen />
	<Features />
</main>
<Footer />
<BackToTop />
