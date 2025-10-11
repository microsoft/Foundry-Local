export class TableOfContents {
	contentRef: HTMLElement | undefined;
	headings: Array<{ id: string; text: string; level: number }> = $state([]);
	activeId: string | null = $state(null);
	private observer: MutationObserver | null = null;
	private intersectionObserver: IntersectionObserver | null = null;
	private static instance: TableOfContents;

	private constructor(contentRef: HTMLElement | undefined) {
		this.contentRef = contentRef;
		if (contentRef) {
			this.initializeObserver();
			this.initializeIntersectionObserver();
		}
	}

	static getInstance(contentRef?: HTMLElement): TableOfContents {
		if (!TableOfContents.instance) {
			TableOfContents.instance = new TableOfContents(contentRef);
		} else if (contentRef) {
			TableOfContents.instance.updateContentRef(contentRef);
		}
		return TableOfContents.instance;
	}

	updateContentRef(newRef: HTMLElement | undefined) {
		this.observer?.disconnect();
		this.intersectionObserver?.disconnect();
		this.contentRef = newRef;
		if (newRef) {
			this.initializeObserver();
			this.initializeIntersectionObserver();
		} else {
			this.headings = [];
			this.activeId = null;
		}
	}

	private extractHeadings() {
		if (!this.contentRef) return;
		const headingElements = this.contentRef.querySelectorAll('h1, h2, h3, h4, h5, h6');

		this.headings = Array.from(headingElements)
			.map((heading) => {
				const level = parseInt(heading.tagName.charAt(1));
				const text = heading.textContent || '';
				const id = heading.id || text.toLowerCase().replace(/[^a-z0-9]+/g, '-');

				if (!heading.id) heading.id = id;

				return { id, text, level };
			})
			.filter(
				(heading): heading is { id: string; text: string; level: number } =>
					heading.text !== null && heading.text !== ''
			);

		// Reapply intersection observer to new headings
		this.initializeIntersectionObserver();
	}

	private initializeIntersectionObserver() {
		this.intersectionObserver?.disconnect();

		if (!this.contentRef || typeof IntersectionObserver === 'undefined') return;

		// Create a map of visible headings and their intersection ratios
		const visibleHeadings = new Map<string, number>();

		this.intersectionObserver = new IntersectionObserver(
			(entries) => {
				entries.forEach((entry) => {
					const id = entry.target.id;
					if (entry.isIntersecting) {
						// Store the intersection ratio for this heading
						visibleHeadings.set(id, entry.intersectionRatio);
					} else {
						// Remove from visible headings
						visibleHeadings.delete(id);
					}
				});

				// Get the heading that's most visible (highest intersection ratio)
				if (visibleHeadings.size > 0) {
					const activeEntry = Array.from(visibleHeadings.entries()).reduce((max, current) =>
						current[1] > max[1] ? current : max
					);
					this.activeId = activeEntry[0];
				} else if (this.headings.length > 0) {
					// If no headings are visible, determine based on scroll position
					this.determineActiveHeadingByScroll();
				} else {
					this.activeId = null;
				}
			},
			{
				root: null, // Use viewport as root
				rootMargin: '-80px 0px -30% 0px', // Adjust margins to consider header height
				threshold: [0, 0.25, 0.5, 0.75, 1] // Multiple thresholds for better accuracy
			}
		);

		// Observe all heading elements
		setTimeout(() => {
			this.headings.forEach((heading) => {
				const element = document.getElementById(heading.id);
				if (element) this.intersectionObserver?.observe(element);
			});
		}, 100); // Small delay to ensure DOM is ready
	}

	private determineActiveHeadingByScroll() {
		if (!this.contentRef || !this.headings.length) return;

		const scrollPosition = window.scrollY + 100; // Add offset for header

		for (let i = this.headings.length - 1; i >= 0; i--) {
			const headingElement = document.getElementById(this.headings[i].id);
			if (headingElement && headingElement.offsetTop <= scrollPosition) {
				this.activeId = this.headings[i].id;
				return;
			}
		}

		// Default to first heading if nothing else matches
		this.activeId = this.headings[0]?.id || null;
	}

	private initializeObserver() {
		this.extractHeadings();

		this.observer = new MutationObserver(() => this.extractHeadings());
		if (!this.contentRef) return;
		this.observer.observe(this.contentRef, {
			childList: true,
			subtree: true
		});
	}

	destroy() {
		this.observer?.disconnect();
		this.intersectionObserver?.disconnect();
	}
}
