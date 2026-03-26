import { type ClassValue, clsx } from 'clsx';
import { twMerge } from 'tailwind-merge';
import type { TransitionConfig } from 'svelte/transition';
import { cubicOut } from 'svelte/easing';

export function cn(...inputs: ClassValue[]): string {
	return twMerge(clsx(inputs));
}

interface FlyAndScaleParams {
	y?: number;
	x?: number;
	start?: number;
	duration?: number;
}

export function styleToString(style: Record<string, number | string | undefined>): string {
	return Object.entries(style)
		.filter(([, value]) => value !== undefined)
		.map(([key, value]) => `${key}:${value};`)
		.join('');
}

/**
 * Linear interpolation between two ranges
 */
function lerp(value: number, [fromMin, fromMax]: [number, number], [toMin, toMax]: [number, number]): number {
	const percentage = (value - fromMin) / (fromMax - fromMin);
	return percentage * (toMax - toMin) + toMin;
}

export function flyAndScale(
	node: Element,
	params: FlyAndScaleParams = { y: -8, x: 0, start: 0.95, duration: 150 }
): TransitionConfig {
	const style = getComputedStyle(node);
	const transform = style.transform === 'none' ? '' : style.transform;

	const { y = 5, x = 0, start = 0.95, duration = 200 } = params;

	return {
		duration,
		delay: 0,
		css: (t) => {
			const translateY = lerp(t, [0, 1], [y, 0]);
			const translateX = lerp(t, [0, 1], [x, 0]);
			const scale = lerp(t, [0, 1], [start, 1]);

			return styleToString({
				transform: `${transform} translate3d(${translateX}px, ${translateY}px, 0) scale(${scale})`,
				opacity: t
			});
		},
		easing: cubicOut
	};
}
