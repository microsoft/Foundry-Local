# Models Page Implementation

## Overview

A comprehensive models page has been created for the Foundry Local website, matching the modern design theme of the site.

## Foundry Models Browser (`/models`)

**Location:** `/src/routes/models/+page.svelte`

An interactive model browser with advanced filtering and search capabilities:

**Features:**

- **Real-time Search** - Search by name, description, or tags with debouncing
- **Device Filtering** - Multi-select filters for NPU, GPU, CPU, FPGA
- **Model Family Filtering** - Filter by DeepSeek, Mistral, Qwen, Phi, etc.
- **Acceleration Filtering** - Filter by QNN, Vitis AI, OpenVINO, TensorRT
- **Sorting** - Sort by name, last modified, or download count
- **Pagination** - 12 models per page with elegant pagination controls
- **Model Cards** - Display key information:
  - Device support badges with icons
  - Task type and license
  - Version and last update date
  - Download counts
  - Copy model ID functionality
- **Loading States** - Skeleton loading and spinners
- **Error Handling** - Graceful error display with retry options
- **Empty States** - Clear messaging when no models match filters

## Supporting Files

### Type Definitions

**Location:** `/src/routes/models/foundry/types.ts`

Defines TypeScript interfaces for:

- `FoundryModel` - Individual model data structure
- `GroupedFoundryModel` - Models grouped by alias
- `Benchmark` - Performance metrics
- `FilterOptions` - Available filter values
- `DEVICE_ICONS` - Icon mappings for devices

### Service Layer

**Location:** `/src/routes/models/foundry/service.ts`

**Class:** `FoundryModelService`

Handles all data fetching and processing:

- **Caching** - Fetches models once and caches for performance
- **API Integration** - Connects to Azure AI Foundry proxy endpoint
- **Model Transformation** - Converts API responses to typed models
- **Client-side Filtering** - Fast filtering without API calls
- **Model Grouping** - Groups device variants by model family
- **Acceleration Detection** - Identifies QNN, Vitis AI, OpenVINO, TensorRT
- **Error Recovery** - Continues fetching from other devices if one fails

**Key Methods:**

- `fetchAllModels()` - Gets and caches all models
- `fetchGroupedModels()` - Returns models grouped by family
- `clearCache()` - Refreshes data from API
- `getAccelerationDisplayName()` - Human-readable acceleration names

## Navigation Integration

The Models page has been added to the main navigation bar in `/src/lib/config.ts`:

```typescript
{
    title: 'Models',
    href: '/models',
    icon: Server
}
```

The link appears in the header navigation alongside Documentation.

## Design Principles

### Theme Consistency

- Uses the site's component library (shadcn/ui for Svelte)
- Matches color scheme (blue/purple primary colors)
- Consistent typography and spacing
- Dark mode support throughout

### User Experience

- Progressive disclosure - Start with overview, drill down to details
- Instant feedback - Real-time search and filtering
- Clear visual hierarchy
- Accessible with proper ARIA labels
- Responsive across all screen sizes

### Performance

- Single API fetch with client-side filtering
- Debounced search to reduce re-renders
- Pagination to limit DOM nodes
- Model caching to prevent redundant requests

## Usage

### Viewing the Models Page

1. Navigate to `/models` to see the model hub overview
2. Click "Explore models" on the Foundry Local Models card
3. Use the search and filters to find specific models
4. Click the copy button to copy model IDs
5. Use pagination to browse through all models

### Refreshing Model Data

Click the "Refresh" button in the filters section to clear cache and fetch fresh data from the API.

### Clearing Filters

Use the "Clear Filters" button to reset all search and filter criteria.

## Technical Stack

- **Framework:** SvelteKit
- **Styling:** TailwindCSS + shadcn/ui components
- **Icons:** Lucide Svelte
- **Notifications:** svelte-sonner (toast)
- **API:** Azure AI Foundry proxy (CORS-enabled)
- **TypeScript:** Full type safety

## Future Enhancements

Potential improvements:

1. Model detail pages with full specifications
2. Download buttons with integration examples
3. Comparison tool for multiple models
4. Benchmark visualization charts
5. User favorites/bookmarking
6. Advanced filters (model size, task type, etc.)
7. Search history and suggestions
8. Model performance ratings
9. Integration code snippets per model
10. Community comments and ratings

## File Structure

```text
src/routes/models/
├── +page.svelte     # Foundry models browser page
├── service.ts       # API service layer
└── types.ts         # TypeScript definitions
```

## Notes

- The original `models/` folder at the root can be safely removed as its content has been migrated to `src/routes/models/`
- All logic from the original implementation has been preserved and enhanced
- The design has been modernized to match the current website theme
- The page is fully functional and ready for production use
- Navigate to `/models` to see the interactive model browser
