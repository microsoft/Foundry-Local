import type { QuickLink } from '$lib/types/nav';
import { type Icon as IconType } from 'lucide-svelte';

export interface Feature {
	icon: typeof IconType;
	title: string;
	description: string;
	size?: 'small' | 'medium' | 'large';
}

export interface SiteConfig {
	/** Main title of the documentation site */
	title: string;

	/** Detailed description of the project/documentation */
	description: string;

	/** GitHub repository URL */
	github: string;

	/** NPM package name */
	npm: string;

	/** Array of quick navigation links */
	quickLinks: QuickLink[];

	/** Path to the main logo (light theme) */
	logo: string;

	/** Path to the dark theme logo (optional) */
	logoDark?: string;

	/** Optional mark or monochrome variant for compact contexts */
	logoMark?: string;

	/** Path to the site favicon */
	favicon: string;
}
export interface PromoConfig {
	title: string;
	description: string;
	ctaText: string;
	ctaLink: string;
	lightImage?: string;
	darkImage?: string;
}
