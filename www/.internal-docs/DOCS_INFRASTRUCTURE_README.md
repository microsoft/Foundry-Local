# Documentation Infrastructure README

This document describes the documentation system that was built for the Foundry Local website. The docs system has been **temporarily disabled** in favor of linking to Microsoft Learn documentation, but all infrastructure has been preserved for potential future reactivation.

## Overview

The documentation system is a Svelte-based markdown documentation site with the following features:

- Markdown file processing with frontmatter metadata
- Dynamic route generation from markdown files
- Navigation sidebar with hierarchical structure
- Search functionality
- Syntax highlighting for code blocks
- Table of contents generation
- Dark mode support

## Directory Structure

```
/home/maanav/Foundry-Local/www/
├── src/
│   ├── content/                      # Markdown documentation files
│   │   ├── index.md                  # Docs homepage
│   │   ├── models.md
│   │   ├── customize.md
│   │   ├── features.md
│   │   ├── examples.md
│   │   └── styling/
│   │       ├── theme.md
│   │       └── syntax-highlighting.md
│   │
│   ├── routes/
│   │   └── docs/                     # Documentation routes
│   │       ├── +page.svelte          # Docs homepage component
│   │       ├── +page.ts              # Loads index.md
│   │       ├── +layout.svelte        # Docs layout (sidebar, TOC)
│   │       ├── +layout.ts            # Navigation & search initialization
│   │       └── [...slug]/            # Dynamic route for all docs pages
│   │           ├── +page.svelte      # Doc page component
│   │           └── +page.ts          # Dynamic doc loader
│   │
│   └── lib/
│       ├── components/
│       │   ├── document/             # Doc-specific components
│       │   │   ├── doc-content.svelte
│       │   │   ├── doc-header.svelte
│       │   │   ├── doc-renderer.svelte
│       │   │   ├── table-of-contents.svelte
│       │   │   ├── mobile-table-of-contents.svelte
│       │   │   ├── promo-card.svelte
│       │   │   └── toc.svelte.ts
│       │   ├── doc-navigation.svelte.ts  # Navigation generation
│       │   ├── doc-search.svelte.ts      # Search index generation
│       │   └── app-sidebar.svelte        # Sidebar component
│       │
│       ├── types/
│       │   └── docs.ts               # TypeScript types for docs
│       │
│       └── utils.ts                  # getDoc() function for loading docs
```

## How It Works

### 1. Markdown Files (`src/content/`)

Documentation is written in Markdown with YAML frontmatter:

```markdown
---
title: 'Page Title'
description: 'Page description'
order: 1
---

# Content Here

Your markdown content with full support for:

- Headers (h1-h6)
- Code blocks with syntax highlighting
- Lists, links, images
- Tables
- Blockquotes
```

### 2. Route Loading (`src/routes/docs/`)

#### Homepage (`+page.ts`)

```typescript
// Loads the index.md file
const doc: DocFile = await import('../../content/index.md');
```

#### Dynamic Pages (`[...slug]/+page.ts`)

```typescript
// Uses getDoc() utility to load markdown files based on URL slug
const doc: DocFile = await getDoc(event.params.slug);
```

#### Layout (`+layout.ts`)

```typescript
// Initializes navigation sidebar and search index
await docsNavigation.generateNavigation();
await docsSearch.initializeSearchIndex();
```

### 3. Components

#### `doc-renderer.svelte`

- Renders the markdown content
- Applies styling and syntax highlighting
- Handles responsive layout

#### `table-of-contents.svelte`

- Generates TOC from markdown headers
- Highlights active section on scroll
- Desktop-only version

#### `mobile-table-of-contents.svelte`

- Mobile dropdown version of TOC

#### `app-sidebar.svelte`

- Navigation sidebar with hierarchical menu
- Collapsible sections
- Active page highlighting

#### `promo-card.svelte`

- Azure AI Foundry promotional card
- Can be embedded in doc pages

### 4. Navigation Generation (`doc-navigation.svelte.ts`)

The navigation system automatically:

1. Scans all markdown files in `src/content/`
2. Reads frontmatter metadata (title, order, etc.)
3. Generates hierarchical navigation structure
4. Orders items based on `order` frontmatter field

### 5. Search System (`doc-search.svelte.ts`)

The search functionality:

1. Indexes all markdown content
2. Creates searchable index from titles, descriptions, and content
3. Provides fuzzy search results
4. Highlights matches

### 6. Utilities (`lib/utils.ts`)

The `getDoc()` function:

- Takes a URL slug as input
- Maps slug to markdown file path
- Dynamically imports the corresponding markdown file
- Returns parsed content and metadata

Example:

```typescript
// URL: /docs/styling/theme
// Loads: src/content/styling/theme.md
const doc = await getDoc('styling/theme');
```

## Configuration Files

### `svelte.config.js`

Configures markdown preprocessing:

```javascript
import { mdsvex } from 'mdsvex';

export default {
	extensions: ['.svelte', '.md'],
	preprocess: [
		mdsvex({
			extensions: ['.md']
			// ... highlighting and other options
		})
	]
};
```

### `vite.config.ts`

Handles markdown file imports and builds

## How to Reactivate

If you want to reactivate the documentation system:

### Step 1: Update Config Links

In `src/lib/config.ts`, change links back to internal routes:

```typescript
quickLinks: [
  { title: 'Getting Started', href: '/docs' },
  { title: 'Model Hub', href: '/docs/models' }
],

navItems: [
  { title: 'Documentation', href: '/docs', icon: BookOpen },
  // ...
]
```

### Step 2: Update Hero Button

In `src/lib/components/home/hero.svelte`:

```svelte
<Button href="/docs" variant="default" size="lg">
	Get Started
	<ChevronRight class="size-4" />
</Button>
```

### Step 3: Update Footer Links

In `src/lib/components/home/footer.svelte`, change back to internal routes:

```html
<a href="/docs">Documentation</a>
<a href="/docs/models">Model Hub</a>
<a href="/docs/examples">Examples</a>
```

### Step 4: Sync Content from Main Docs

Copy markdown files from `/home/maanav/Foundry-Local/docs/` to `/home/maanav/Foundry-Local/www/src/content/`:

```bash
# Example structure mapping:
# docs/README.md -> src/content/index.md
# docs/what-is-foundry-local.md -> src/content/what-is-foundry-local.md
# docs/how-to/*.md -> src/content/how-to/*.md
```

### Step 5: Add Frontmatter to Markdown Files

Ensure all markdown files have proper frontmatter:

```yaml
---
title: 'Getting Started'
description: 'Learn how to install and use Foundry Local'
order: 1
---
```

### Step 6: Test Navigation and Search

```bash
npm run dev
# Navigate to http://localhost:5173/docs
# Test navigation, search, and page rendering
```

## Current State (Disabled)

The docs routes are currently **active but not linked** from the main navigation. Direct navigation to `/docs` will still work, but users are directed to Microsoft Learn via:

- Hero "Get Started" button → Microsoft Learn
- Config quickLinks → Microsoft Learn
- Footer links → Microsoft Learn
- Navigation menu "Documentation" → Microsoft Learn

## Dependencies

Required packages for the docs system:

- `mdsvex` - Markdown preprocessor for Svelte
- `rehype-*` and `remark-*` plugins for markdown processing
- `shiki` or `prism` for syntax highlighting
- `lucide-svelte` for icons

Check `package.json` for complete list.

## Notes

- All infrastructure is **preserved and functional**
- No code has been deleted, only navigation links changed
- The docs can be reactivated with minimal changes
- Consider keeping both systems: internal docs for developer/technical content, Microsoft Learn for user-facing guides

## Questions?

For questions about this infrastructure, see:

- SvelteKit docs: https://kit.svelte.dev/
- mdsvex docs: https://mdsvex.pngwn.io/
- The original implementation in the codebase
