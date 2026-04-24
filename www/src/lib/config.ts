import type { NavItem, SocialLink } from '$lib/types/nav';
import {
	Boxes,
	Paintbrush,
	Workflow,
	Zap,
	Rocket,
	Box,
	Code,
	Cpu,
	Bot,
	BookOpen,
	Wifi,
	Shield
} from 'lucide-svelte';
import type { Feature, PromoConfig, SiteConfig } from './types/config';

export const siteConfig: SiteConfig = {
	title: 'Foundry Local',
	description:
		'Ship AI-powered apps with on-device inference. Native SDKs for C#, JavaScript, Python, and Rust. ~20 MB runtime, no cloud required.',
	github: 'https://github.com/microsoft/foundry-local',
	npm: '',
	quickLinks: [
		{
			title: 'Getting Started',
			href: 'https://learn.microsoft.com/en-us/azure/foundry-local/get-started'
		},
		{ title: 'GitHub', href: 'https://github.com/microsoft/foundry-local' }
	],
	logo: '/logos/foundry-local-logo-color.svg',
	logoDark: '/logos/foundry-local-logo-fill.svg',
	logoMark: '/logos/foundry-local-logo-stroke.svg',
	favicon: '/favicon.png'
};

export let navItems: NavItem[] = [
	{
		title: 'Models',
		href: '/models',
		icon: Box
	},
	{
		title: 'Docs',
		href: 'https://learn.microsoft.com/en-us/azure/foundry-local/get-started',
		icon: BookOpen
	}
];

export let socialLinks: SocialLink[] = [
	{
		title: 'GitHub',
		href: 'https://github.com/microsoft/foundry-local',
		icon: 'github'
	}
];

export const features: Feature[] = [
	{
		icon: Rocket,
		title: 'Ship to Production',
		description:
			'Built as an SDK for shipping AI-powered applications, not just running models locally',
		size: 'large'
	},
	{
		icon: Cpu,
		title: 'Hardware Optimized',
		description: 'Automatic execution provider management for NPU, GPU & CPU acceleration',
		size: 'large'
	},
	{
		icon: Wifi,
		title: 'Edge-Ready',
		description: 'Works fully offline. ~20 MB runtime, no cloud dependencies',
		size: 'medium'
	},
	{
		icon: Code,
		title: 'Multi-Language SDKs',
		description: 'Python, JavaScript, C#, and Rust SDKs',
		size: 'medium'
	},
	{
		icon: Bot,
		title: 'OpenAI Compatible',
		description: 'Native SDK API plus optional OpenAI-compatible REST server',
		size: 'medium'
	},
	{
		icon: Shield,
		title: 'Data Privacy',
		description: 'Everything stays on-device',
		size: 'medium'
	}
];

export let promoConfig: PromoConfig = {
	title: 'Need to scale to the cloud?',
	description:
		'Azure AI Foundry provides enterprise-scale AI infrastructure when your project outgrows local deployment.',
	ctaText: 'Learn about Azure AI Foundry',
	ctaLink: 'https://azure.microsoft.com/en-us/products/ai-studio',
	lightImage: '/images/cloud-scale-light.jpg',
	darkImage: '/images/cloud-scale-dark.jpg'
};
