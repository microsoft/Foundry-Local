// Animation utilities for Svelte components
import type { Action } from 'svelte/action';

// Intersection Observer options
interface AnimateOptions {
	delay?: number;
	duration?: number;
	threshold?: number;
	once?: boolean;
	animation?: 'fade-in' | 'slide-up' | 'slide-down' | 'slide-left' | 'slide-right' | 'scale';
}

// Svelte action for scroll-triggered animations
export const animate: Action<HTMLElement, AnimateOptions> = (
	node: HTMLElement,
	options: AnimateOptions = {}
) => {
	const {
		delay = 0,
		duration = 600,
		threshold = 0.1,
		once = true,
		animation = 'fade-in'
	} = options;

	// Set initial state based on animation type
	const initialStyles = getInitialStyles(animation);
	Object.assign(node.style, initialStyles);

	node.style.transition = `all ${duration}ms cubic-bezier(0.4, 0, 0.2, 1)`;
	node.style.transitionDelay = `${delay}ms`;

	const triggerAnimation = () => {
		requestAnimationFrame(() => {
			Object.assign(node.style, {
				opacity: '1',
				transform: 'translate(0, 0) scale(1)'
			});
		});
	};

	// Check if element is already in viewport
	const rect = node.getBoundingClientRect();
	const isInViewport = rect.top < window.innerHeight && rect.bottom > 0;

	if (isInViewport) {
		// Trigger animation immediately if already visible
		triggerAnimation();
	}

	const observer = new IntersectionObserver(
		(entries) => {
			entries.forEach((entry) => {
				if (entry.isIntersecting) {
					// Trigger animation
					triggerAnimation();

					if (once) {
						observer.unobserve(node);
					}
				} else if (!once) {
					// Reset animation if not "once"
					Object.assign(node.style, initialStyles);
				}
			});
		},
		{ threshold }
	);

	observer.observe(node);

	return {
		destroy() {
			observer.disconnect();
		}
	};
};

// Stagger animation for list items
export function staggerAnimation(
	container: HTMLElement,
	options: { delay?: number; staggerDelay?: number; animation?: AnimateOptions['animation'] } = {}
) {
	const { delay = 0, staggerDelay = 100, animation = 'fade-in' } = options;
	const children = Array.from(container.children) as HTMLElement[];

	children.forEach((child, index) => {
		const childDelay = delay + index * staggerDelay;
		const initialStyles = getInitialStyles(animation);

		Object.assign(child.style, initialStyles);
		child.style.transition = `all 600ms cubic-bezier(0.4, 0, 0.2, 1)`;
		child.style.transitionDelay = `${childDelay}ms`;
	});

	const triggerAnimation = () => {
		children.forEach((child) => {
			requestAnimationFrame(() => {
				Object.assign(child.style, {
					opacity: '1',
					transform: 'translate(0, 0) scale(1)'
				});
			});
		});
	};

	// Check if element is already in viewport
	const rect = container.getBoundingClientRect();
	const isInViewport = rect.top < window.innerHeight && rect.bottom > 0;

	if (isInViewport) {
		// Trigger animation immediately if already visible
		triggerAnimation();
	}

	const observer = new IntersectionObserver(
		(entries) => {
			entries.forEach((entry) => {
				if (entry.isIntersecting) {
					triggerAnimation();
					observer.unobserve(container);
				}
			});
		},
		{ threshold: 0.1 }
	);

	observer.observe(container);

	return () => {
		observer.disconnect();
	};
}

// Helper function to get initial styles based on animation type
function getInitialStyles(animation: AnimateOptions['animation']): Partial<CSSStyleDeclaration> {
	const styles: any = {
		opacity: '0'
	};

	switch (animation) {
		case 'slide-up':
			styles.transform = 'translateY(30px)';
			break;
		case 'slide-down':
			styles.transform = 'translateY(-30px)';
			break;
		case 'slide-left':
			styles.transform = 'translateX(30px)';
			break;
		case 'slide-right':
			styles.transform = 'translateX(-30px)';
			break;
		case 'scale':
			styles.transform = 'scale(0.95)';
			break;
		case 'fade-in':
		default:
			styles.transform = 'translateY(0)';
			break;
	}

	return styles;
}

// Smooth scroll utility
export function smoothScroll(target: string | HTMLElement, offset = 0) {
	const element = typeof target === 'string' ? document.querySelector(target) : target;

	if (!element) return;

	const targetPosition = element.getBoundingClientRect().top + window.pageYOffset - offset;

	window.scrollTo({
		top: targetPosition,
		behavior: 'smooth'
	});
}

// Page transition helpers
export const pageTransition = {
	duration: 300,
	css: (t: number) => `
		opacity: ${t};
		transform: scale(${0.98 + t * 0.02});
	`
};

export const slideTransition = {
	duration: 300,
	css: (t: number) => `
		opacity: ${t};
		transform: translateY(${(1 - t) * 20}px);
	`
};
