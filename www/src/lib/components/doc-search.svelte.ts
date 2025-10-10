import FlexSearch from 'flexsearch';
import type { DocFile } from '$lib/types/docs';

interface SearchDocument {
	id: string;
	title: string;
	description: string;
	content: string;
	slug: string;
}

interface SearchResult {
	title: string;
	slug: string;
	description: string;
}

type FlexSearchDocument = {
	id: string;
	[key: string]: any;
};

class DocsSearch {
	private static instance: DocsSearch;
	private index: any; // Using any for index type to avoid complex FlexSearch typing
	private documents: Map<string, SearchDocument>;
	public searchResults = $state<SearchResult[]>([]);
	public isSearching = $state<boolean>(false);

	private constructor() {
		this.index = new FlexSearch.Document({
			document: {
				id: 'id',
				index: ['title', 'content'],
				store: ['title', 'slug']
			},
			tokenize: 'forward',
			context: true,
			cache: true
		});
		this.documents = new Map();
	}

	public static getInstance(): DocsSearch {
		if (!DocsSearch.instance) {
			DocsSearch.instance = new DocsSearch();
		}
		return DocsSearch.instance;
	}

	public async addDocument(doc: DocFile, slug: string): Promise<void> {
		if (!doc.metadata || !doc.default) return;

		const searchDoc: SearchDocument = {
			id: slug,
			title: doc.metadata.title,
			description: doc.metadata.description || '',
			content: '',
			slug: slug
		};
		this.documents.set(slug, searchDoc);
		await this.index.add(searchDoc);
	}

	public async search(query: string): Promise<void> {
		if (!query || query.length < 2) {
			this.searchResults = [];
			this.isSearching = false;
			return;
		}

		try {
			const results: Array<{ field: string; result: Array<FlexSearchDocument> }> =
				await this.index.search(query, {
					limit: 10,
					enrich: true
				});

			// Flatten and deduplicate results
			const searchResults = new Set<string>();
			const formattedResults: SearchResult[] = [];

			results.forEach((result) => {
				result.result.forEach((doc) => {
					const docId = doc.id as string;
					if (!searchResults.has(docId)) {
						searchResults.add(docId);
						const document = this.documents.get(docId);
						if (document) {
							formattedResults.push({
								title: document.title,
								slug: document.slug,
								description: document.description
							});
						}
					}
				});
			});

			this.searchResults = formattedResults;
		} finally {
			setTimeout(() => {
				this.isSearching = false;
			}, 500);
		}
	}

	public async initializeSearchIndex(): Promise<void> {
		const modules = import.meta.glob(['/src/content/**/*.md', '/src/content/**/*.svx'], {
			eager: true
		});

		for (const [path, module] of Object.entries(modules)) {
			try {
				const doc = module as DocFile;
				const slug = path
					.replace('/src/content/', '')
					.replace(/\.(md|svx)$/, '')
					.replace(/\/index$/, '');
				await this.addDocument(doc, slug);
			} catch (e) {
				console.error(`Error indexing ${path}:`, e);
			}
		}
	}

	public clearSearch(): void {
		this.searchResults = [];
		this.isSearching = false;
	}
}

export const docsSearch = DocsSearch.getInstance();
