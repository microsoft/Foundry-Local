# Animation Visual Guide

## Overview

This guide shows what animations were added where on the Foundry Local website.

---

## 🏠 Home Page (`/`)

### Hero Section

```
┌─────────────────────────────────────────┐
│                                         │
│   "Foundry Local"  ⬆️ SLIDE UP (0ms)   │
│                                         │
│   Description text ⬆️ SLIDE UP (200ms)  │
│                                         │
│   💻 CPU  🧠 NPU  🎮 GPU               │
│   [Staggered fade-in: 600ms base]       │
│                                         │
│   [Get Started] [Download]              │
│   [Staggered fade-in: 900ms base]       │
│   [Hover: Scale 1.05 + Shadow]          │
│                                         │
└─────────────────────────────────────────┘
```

### Features Section

```
┌─────────────────────────────────────────┐
│ Section Title ⬆️ SLIDE UP               │
│ Section Description ⬆️ SLIDE UP (200ms) │
│                                         │
│ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐       │
│ │ 🔒  │ │ 💰  │ │ ⚡  │ │ 🔧  │       │
│ │Card1│ │Card2│ │Card3│ │Card4│       │
│ └─────┘ └─────┘ └─────┘ └─────┘       │
│ [Staggered: 0ms, 100ms, 200ms, 300ms]  │
│ [Hover: Lift + Icon rotate + Scale]     │
│                                         │
│ ┌─────────────────┐ ┌─────────────────┐│
│ │ Data Privacy    │ │ API Compatible  ││
│ │ Section         │ │ Section         ││
│ └─────────────────┘ └─────────────────┘│
│ [Both: Slide up on scroll + Hover FX]  │
└─────────────────────────────────────────┘
```

---

## 🔍 Models Page (`/models`)

### Hero

```
┌─────────────────────────────────────────┐
│   "Foundry Local Models" ⬆️ SLIDE UP    │
│   Description ⬆️ SLIDE UP (200ms)       │
└─────────────────────────────────────────┘
```

### Filters & Cards

```
┌─────────────────────────────────────────┐
│ [Search Filters] 💫 FADE IN             │
└─────────────────────────────────────────┘

┌─────┐ ┌─────┐ ┌─────┐
│Model│ │Model│ │Model│
│ #1  │ │ #2  │ │ #3  │
└─────┘ └─────┘ └─────┘
[Each: Fade in on scroll]
[Hover: Lift -1px + Shadow XL + Border]
```

---

## 📚 Documentation Pages (`/docs/*`)

### Layout

```
┌────────────────────────────────────────┐
│ [Sidebar] │ [Header with Backdrop Blur]│
│           │                            │
│  Nav      │  [Doc Content]             │
│  Items    │  💫 FADE IN                │
│           │                            │
└────────────────────────────────────────┘
```

---

## 🧭 Navigation (All Pages)

### Desktop Nav

```
┌─────────────────────────────────────────┐
│ 🏠 Logo  [Docs] [Models] [Download]    │
│ [Fade in on load + Backdrop blur 80%]  │
│ [Logo hover: Scale 1.05]                │
└─────────────────────────────────────────┘
```

### Mobile Nav

```
┌─────────────────┐
│ 🏠 Logo  ≡      │
└─────────────────┘
        ↓ Click
┌─────────────────┐
│ 🏠 Logo  ≡      │
├─────────────────┤
│ ↓ Slide Down    │
│ [Docs]          │
│ [Models]        │
│ [Download]      │
└─────────────────┘
```

---

## 🎨 UI Component Animations

### Buttons

- **Hover**: Shadow enhancement + slight scale
- **Active**: Scale down to 0.95
- **Transition**: 300ms cubic-bezier

### Cards

- **Hover**: Shadow increase
- **Transition**: 300ms smooth

### Badges

- **Hover**: Scale 1.05 + Shadow
- **Transition**: 300ms

### Inputs

- **Hover**: Border color → primary/50
- **Focus**: Border → primary + ring
- **Transition**: 300ms

### Skeletons

- **Loading**: Pulse + Shimmer gradient
- **Continuous**: Infinite animation

---

## ⏱️ Animation Timings

### Quick Reference

```
Fast:    200-300ms  (Hover effects, inputs)
Medium:  600ms      (Scroll animations)
Slow:    800ms      (Hero elements)
```

### Stagger Delays

```
Badges:  600ms base + 100ms per item
Buttons: 900ms base + 100ms per item
Cards:   0ms base + 100ms per card
```

---

## 🎯 Hover Effects Zones

### Scale Effects

- ✅ Buttons (1.05)
- ✅ Badges (1.05)
- ✅ Logo (1.05)
- ✅ Feature card icons (1.10)

### Lift Effects (translateY)

- ✅ Feature cards (-1px)
- ✅ Model cards (-1px)
- ✅ Extended sections (shadow only)

### Rotation

- ✅ Feature card icons (6deg)

### Shadow Enhancement

- ✅ Buttons (default → md)
- ✅ Cards (sm → lg/xl)
- ✅ Extended sections (none → xl)

---

## 📱 Responsive Animations

All animations work consistently across:

- ✅ Desktop (1920px+)
- ✅ Laptop (1024px - 1919px)
- ✅ Tablet (768px - 1023px)
- ✅ Mobile (320px - 767px)

---

## 🔧 Implementation Pattern

```svelte
<!-- Staggered list -->
<script>
	onMount(() => {
		staggerAnimation(container, {
			staggerDelay: 100
		});
	});
</script>

<!-- Scroll-triggered animation -->
<div
	use:animate={{
		delay: 0,
		duration: 600,
		animation: 'fade-in'
	}}
>
	Content
</div>

<!-- Hover animation (CSS) -->
<button class="transition-all duration-300 hover:scale-105"> Click me </button>
```

---

## 💡 Pro Tips

1. **Keep it subtle**: Animations should enhance, not distract
2. **Stay consistent**: Use the same easing function everywhere
3. **Test performance**: Check on lower-end devices
4. **Respect preferences**: Consider adding reduced motion support
5. **Document changes**: Update this guide when adding new animations

---

## 🚀 Next Steps

To add animations to a new component:

1. Import the animation utility:

   ```typescript
   import { animate } from '$lib/utils/animations';
   ```

2. Add the action to your element:

   ```svelte
   <div use:animate={{ animation: 'fade-in', duration: 600 }}>
   ```

3. Or use CSS classes:

   ```svelte
   <div class="transition-all duration-300 hover:scale-105">
   ```

4. Test on multiple devices and screen sizes

5. Update this guide with your new animation!
