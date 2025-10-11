# Animation System Documentation

This document describes the comprehensive animation system implemented across the Foundry Local website.

## Overview

The animation system provides smooth, professional animations that enhance user experience without being distracting. All animations follow these principles:

- **Performance-first**: Using CSS transforms and opacity for GPU acceleration
- **Accessibility-aware**: Respecting user motion preferences
- **Consistent timing**: Using cubic-bezier(0.4, 0, 0.2, 1) easing for natural motion
- **Progressive enhancement**: Animations degrade gracefully on older browsers

## Animation Utilities

### Location: `/src/lib/utils/animations.ts`

#### `animate` - Svelte Action

A reusable Svelte action for scroll-triggered animations using Intersection Observer.

**Usage:**
```svelte
<div use:animate={{ delay: 0, duration: 600, animation: 'fade-in' }}>
  Content
</div>
```

**Options:**
- `delay` (number): Delay in milliseconds before animation starts (default: 0)
- `duration` (number): Animation duration in milliseconds (default: 600)
- `threshold` (number): Intersection Observer threshold (default: 0.1)
- `once` (boolean): Whether animation should only run once (default: true)
- `animation` (string): Animation type - 'fade-in', 'slide-up', 'slide-down', 'slide-left', 'slide-right', 'scale'

#### `staggerAnimation`

Creates staggered animations for lists of elements.

**Usage:**
```typescript
onMount(() => {
  if (container) {
    staggerAnimation(container, { 
      delay: 0, 
      staggerDelay: 100, 
      animation: 'fade-in' 
    });
  }
});
```

## CSS Animations

### Location: `/src/app.css`

### Keyframe Animations

1. **slideDown**: Slides element down with fade-in
2. **slideUp**: Slides element up with fade-in
3. **fadeIn**: Simple fade-in animation
4. **scaleIn**: Scales element from 95% to 100% with fade-in
5. **shimmer**: Loading shimmer effect for skeletons

### Utility Classes

- `.animate-slideDown`: Apply slideDown animation
- `.animate-slideUp`: Apply slideUp animation
- `.animate-fadeIn`: Apply fadeIn animation
- `.animate-scaleIn`: Apply scaleIn animation
- `.animate-shimmer`: Apply shimmer effect

### Component Classes

- `.transition-smooth`: Smooth 300ms transition
- `.hover-lift`: Lift effect on hover
- `.hover-scale`: Scale effect on hover
- `.hover-glow`: Glow effect on hover

## Component Animations

### Hero Component (`/src/lib/components/home/hero.svelte`)

**Animations:**
- Title: Slide-up animation (0ms delay, 800ms duration)
- Description: Slide-up animation (200ms delay, 800ms duration)
- Badges: Staggered fade-in (600ms base delay, 100ms stagger)
- Buttons: Staggered fade-in (900ms base delay, 100ms stagger)
- Hover effects on badges and buttons (scale + shadow)

### Features Component (`/src/lib/components/home/features.svelte`)

**Animations:**
- Section header: Slide-up animation
- Section description: Slide-up animation (200ms delay)
- Feature cards: Staggered fade-in (100ms stagger per card)
- Extended sections: Slide-up with hover effects
- Hover effects: Card lift, icon scale, icon rotation

### Navigation Component (`/src/lib/components/home/nav.svelte`)

**Animations:**
- Navbar: Fade-in slide-down on mount
- Logo: Scale effect on hover
- Mobile menu: slideDown animation when opened
- Backdrop blur effect on scroll

### Models Page (`/src/routes/models/+page.svelte`)

**Animations:**
- Page hero: Slide-up animations for title and description
- Filter card: Fade-in animation
- Model cards: Fade-in on scroll with hover lift effect
- Hover effects: Card lift, border highlight, shadow enhancement

### Footer Component (`/src/lib/components/home/footer.svelte`)

**Animations:**
- Fade-in when scrolling into view

### Docs Layout (`/src/routes/docs/+layout.svelte`)

**Animations:**
- Header backdrop blur effect
- Content fade-in animations

### Doc Content (`/src/lib/components/document/doc-content.svelte`)

**Animations:**
- Content area fade-in on load

## UI Component Enhancements

### Button Component (`/src/lib/components/ui/button/button.svelte`)

**Enhancements:**
- Transition duration: 300ms
- Active state: scale(0.95)
- Hover effects: Enhanced shadows
- All transitions: cubic-bezier(0.4, 0, 0.2, 1)

### Card Component (`/src/lib/components/ui/card/card.svelte`)

**Enhancements:**
- Smooth transitions on all properties
- Duration: 300ms

### Badge Component (`/src/lib/components/ui/badge/badge.svelte`)

**Enhancements:**
- Hover scale: 105%
- Enhanced shadows on hover
- Transition duration: 300ms

### Input Component (`/src/lib/components/ui/input/input.svelte`)

**Enhancements:**
- Hover border color transition
- Focus border highlight
- Transition duration: 300ms

### Skeleton Component (`/src/lib/components/ui/skeleton/skeleton.svelte`)

**Enhancements:**
- Shimmer effect for better loading indication
- Gradient animation using CSS keyframes

## Page Transitions

### Location: `/src/routes/+layout.svelte`

**Implementation:**
- Uses native View Transitions API
- Smooth fade and scale effect between pages
- Gracefully degrades if API not supported

**Usage:**
```typescript
onNavigate((navigation) => {
  if (!document.startViewTransition) return;
  return new Promise((resolve) => {
    document.startViewTransition(async () => {
      resolve();
      await navigation.complete;
    });
  });
});
```

## Performance Considerations

1. **Intersection Observer**: Used for scroll-triggered animations to efficiently detect element visibility
2. **RequestAnimationFrame**: Used for smooth animation triggers
3. **CSS Transforms**: All position animations use transform instead of position properties
4. **Will-change**: Avoided to prevent performance issues
5. **GPU Acceleration**: Transform and opacity properties trigger GPU acceleration

## Browser Support

- Modern browsers: Full support
- Safari: Full support (including View Transitions in Safari 18+)
- Firefox: Full support (View Transitions behind flag)
- Chrome/Edge: Full support

## Best Practices

1. **Keep animations subtle**: Animations should enhance, not distract
2. **Respect reduced motion**: Consider adding `prefers-reduced-motion` media query
3. **Test performance**: Monitor animation performance on lower-end devices
4. **Consistent timing**: Use the same easing function throughout
5. **Meaningful motion**: Animations should have purpose and meaning

## Future Enhancements

Potential improvements to consider:

1. Add `prefers-reduced-motion` media query support
2. Create animation presets for common patterns
3. Add animation configuration options
4. Implement animation sequencing utilities
5. Add spring-based animations for more natural motion
6. Create animation documentation site with live examples

## Testing

When testing animations:

1. Verify animations work on page load
2. Test scroll-triggered animations at different scroll speeds
3. Check animations on mobile devices
4. Test with different browser zoom levels
5. Verify performance with DevTools Performance tab
6. Test with reduced motion preferences enabled
