import type { NavItem, SocialLink } from '$lib/types/nav';
import {
	Boxes,
	Paintbrush,
	Workflow,
	Zap,
	Server,
	Box,
	Code,
	Cloud,
	Bot,
	BookOpen
} from 'lucide-svelte';
import type { Feature, PromoConfig, SiteConfig } from './types/config';

export const siteConfig: SiteConfig = {
	title: 'Foundry Local',
	description:
		'Run AI models locally on your device. Foundry Local provides on-device inference with complete data privacy, no Azure subscription required.',
	github: 'https://github.com/microsoft/foundry-local',
	npm: '',
	quickLinks: [
		{
			title: 'Getting Started',
			href: 'https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started'
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
		href: 'https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started',
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
		icon: Server,
		title: 'On-Device Inference',
		description:
			'Run AI models directly on your device with no cloud dependencies or Azure subscription required'
	},
	{
		icon: Bot,
		title: 'Optimized Performance',
		description: 'Powered by ONNX Runtime with hardware acceleration for CPUs, GPUs, and NPUs'
	},
	{
		icon: Cloud,
		title: 'OpenAI-Compatible API',
		description: 'Easy integration with existing applications using familiar OpenAI patterns'
	},
	{
		icon: Code,
		title: 'Multi-Language SDKs',
		description:
			'Simple SDKs for Python, JavaScript, C#, and Rust to get started quickly with your applications'
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
