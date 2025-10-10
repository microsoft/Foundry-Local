import adapter from '@sveltejs/adapter-auto';
import { vitePreprocess } from '@sveltejs/vite-plugin-svelte';
import { mdsvex } from 'mdsvex';

/** @type {import('@sveltejs/kit').Config} */
const config = {
	extensions: ['.svelte', '.md', '.svx'],
	preprocess: [
		vitePreprocess({}),
		mdsvex({
			extensions: ['.md', '.svx']
		})
	],

	kit: {
		adapter: adapter(),
		prerender: {
			handleHttpError: ({ path, referrer, message }) => {
				// Ignore 404s for missing routes during prerendering
				if (path !== '/docs/styling') {
					console.warn(`Warning: ${path} returned 404 (linked from ${referrer})`);
				}
			}
		}
	}
};

export default config;
