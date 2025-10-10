import { type Icon as IconType } from 'lucide-svelte';

export interface NavItem {
	title: string;
	href?: string;
	disabled?: boolean;
	external?: boolean;
	icon?: typeof IconType;
	label?: string;
	items?: NavItem[];
}

export interface SocialLink {
	title: string;
	href: string;
	icon: keyof Icons;
}

export interface Icons {
	twitter: string;
	github: string;
	facebook: string;
	instagram: string;
	linkedin: string;
	youtube: string;
	tiktok: string;
	snapchat: string;
}

export interface QuickLink {
	title: string;
	href: string;
}
