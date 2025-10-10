import type { Component } from 'svelte';

export type Metadata = {
	title: string;
	description: string;
	openGraph: {
		title: string;
		description: string;
		type: 'article';
		url: string;
		images: [
			{
				url: string;
				width: number;
				height: number;
				alt: string;
			}
		];
	};
	twitter: {
		card: 'summary_large_image';
		title: string;
		description: string;
		images: string[];
		creator: string;
	};
};

export type DocMeta = {
	title: string;
	description: string;
	slug: string;
	component: boolean;
	source: string;
	bits?: string;
	icon?: string;
	label?: string;
	disabled?: false;
	external?: false;
};

export type DocFile = {
	default: Component;
	metadata: DocMeta;
};

export type DocResolver = () => Promise<DocFile>;

export type TableOfContentsItem = {
	title: string;
	url: string;
	items?: TableOfContentsItem[];
};

export type TableOfContents = {
	items: TableOfContentsItem[];
};
