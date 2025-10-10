import { docsNavigation } from '$lib/components/doc-navigation.svelte';
import { docsSearch } from '$lib/components/doc-search.svelte';

export async function load() {
	await docsNavigation.generateNavigation();
	await docsSearch.initializeSearchIndex();
	return {};
}
