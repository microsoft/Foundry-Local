import type { DocResolver } from '$lib/types/docs';
import type { NavItem } from '$lib/types/nav';

class DocsNavigation {
	private static instance: DocsNavigation;
	public docNav = $state<NavItem[]>([]);

	private constructor() {}

	public static getInstance(): DocsNavigation {
		if (!DocsNavigation.instance) {
			DocsNavigation.instance = new DocsNavigation();
		}
		return DocsNavigation.instance;
	}

	public async generateNavigation(): Promise<NavItem[]> {
		const modules = import.meta.glob(`/src/content/**/*.md`);
		const flatItems: NavItem[] = [];

		// First, collect all items in a flat structure
		for (const [path, resolver] of Object.entries(modules)) {
			try {
				const doc = await (resolver as DocResolver)();
				if (!doc?.metadata?.title) continue;

				let href = `/docs${path.replace(/^\/src\/content/, '').replace(/\.md$/, '')}`;
				href = href.replace(/\/index$/, '');

				const { title, disabled, external, label } = doc.metadata;
				const item: NavItem = {
					title,
					href,
					disabled,
					external,
					label
				};

				flatItems.push(item);
			} catch (e) {
				console.error(`Error processing ${path}:`, e);
			}
		}

		const nestedItems = this.createNestedStructure(flatItems);
		this.docNav = this.cleanupEmptyItems(nestedItems);
		return this.docNav;
	}

	private createNestedStructure(items: NavItem[]): NavItem[] {
		const root: NavItem[] = [];
		const folderMap = new Map<string, NavItem>();
		const allPaths = items.map((i) => i.href);
		// Sort items so parent paths come before children
		items.sort((a, b) => (a.href?.split('/').length ?? 0) - (b.href?.split('/').length ?? 0));

		items.forEach((item) => {
			if (!item.href) {
				root.push(item);
				return;
			}

			const pathParts = item.href.split('/').filter(Boolean);

			// Skip the 'docs' part as it's common to all
			if (pathParts[0] === 'docs') {
				pathParts.shift();
			}

			if (pathParts.length === 0) {
				// This is the root level index
				root.push(item);
			} else {
				const currentFolder = pathParts[0];
				const isIndex = pathParts.length === 1;
				// Filter paths that start with this folder
				const childPaths = allPaths.filter(
					(path) => path?.startsWith(`/docs/${currentFolder}/`) || path === `/docs/${currentFolder}`
				);

				// A folder is considered single if it has exactly one entry (itself)
				const isSingle = childPaths.length === 1;
				if (isIndex && isSingle) {
					// This is a root level item
					root.push(item);
				} else {
					// Check if we need to create a folder
					if (!folderMap.has(currentFolder)) {
						const folderItem: NavItem = {
							title: currentFolder.charAt(0).toUpperCase() + currentFolder.slice(1),
							items: []
						};
						folderMap.set(currentFolder, folderItem);
						root.push(folderItem);
					}

					const folder = folderMap.get(currentFolder);
					if (folder?.items) {
						folder.items.push(item);
					}
				}
			}
		});
		return this.sortNavItems(root);
	}

	// Clean up empty items arrays
	private cleanupEmptyItems(items: NavItem[]): NavItem[] {
		return items.map((item) => {
			if (item.items?.length === 0) {
				const { items, ...rest } = item;
				return rest;
			} else if (item.items) {
				return {
					...item,
					items: this.cleanupEmptyItems(item.items)
				};
			}
			return item;
		});
	}

	private sortNavItems(items: NavItem[]): NavItem[] {
		return [...items]
			.sort((a, b) => {
				// If both items have sub-items or neither have sub-items, maintain current order
				if ((!a.items && !b.items) || (a.items && b.items)) {
					return 0;
				}
				// Items without sub-items should come first
				return a.items ? 1 : -1;
			})
			.map((item) => {
				// Recursively sort sub-items if they exist
				if (item.items) {
					return {
						...item,
						items: this.sortNavItems(item.items)
					};
				}
				return item;
			});
	}
}

export const docsNavigation = DocsNavigation.getInstance();
