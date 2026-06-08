<script lang="ts">
	import { Toaster as Sonner, type ToasterProps as SonnerProps } from 'svelte-sonner';
	import { mode } from 'mode-watcher';

	let restProps: SonnerProps = $props();
</script>

<Sonner
	theme={mode.current}
	class="toaster group"
	toastOptions={{
		duration: 2400,
		classes: {
			toast: 'toast group border-border bg-background text-foreground shadow-lg',
			success: 'toast-success',
			error: 'toast-error',
			info: 'toast-info',
			warning: 'toast-warning',
			description: 'group-[.toast]:text-muted-foreground',
			icon: 'toast-icon',
			actionButton: 'group-[.toast]:bg-primary group-[.toast]:text-primary-foreground',
			cancelButton: 'group-[.toast]:bg-muted group-[.toast]:text-muted-foreground'
		}
	}}
	{...restProps}
/>

<style>
	:global([data-sonner-toast].toast) {
		border-color: hsl(var(--border));
		background: hsl(var(--background));
		color: hsl(var(--foreground));
		box-shadow:
			0 8px 24px hsl(var(--foreground) / 0.08),
			0 1px 2px hsl(var(--foreground) / 0.08);
	}

	:global([data-sonner-toast].toast [data-title]) {
		font-weight: 500;
	}

	:global([data-sonner-toast].toast [data-description]) {
		color: hsl(var(--muted-foreground));
	}

	:global([data-sonner-toast].toast .toast-icon) {
		color: hsl(var(--primary));
	}

	:global([data-sonner-toast].toast-success) {
		border-color: hsl(var(--success) / 0.35);
		background:
			linear-gradient(90deg, hsl(var(--success) / 0.14), hsl(var(--success) / 0.06)),
			hsl(var(--background));
	}

	:global([data-sonner-toast].toast-success .toast-icon) {
		color: hsl(var(--success));
	}

	:global([data-sonner-toast].toast-error) {
		border-color: hsl(var(--destructive) / 0.35);
		background:
			linear-gradient(90deg, hsl(var(--destructive) / 0.12), hsl(var(--destructive) / 0.05)),
			hsl(var(--background));
	}

	:global([data-sonner-toast].toast-error .toast-icon) {
		color: hsl(var(--destructive));
	}

	:global([data-sonner-toast].toast-info) {
		border-color: hsl(var(--info) / 0.35);
		background:
			linear-gradient(90deg, hsl(var(--info) / 0.12), hsl(var(--info) / 0.05)),
			hsl(var(--background));
	}

	:global([data-sonner-toast].toast-info .toast-icon) {
		color: hsl(var(--info));
	}

	:global([data-sonner-toast].toast-warning) {
		border-color: hsl(var(--warning) / 0.4);
		background:
			linear-gradient(90deg, hsl(var(--warning) / 0.14), hsl(var(--warning) / 0.05)),
			hsl(var(--background));
	}

	:global([data-sonner-toast].toast-warning .toast-icon) {
		color: hsl(var(--warning));
	}
</style>
