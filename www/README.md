# Foundry Local Website

Official marketing website for Foundry Local - Microsoft's on-device AI inference platform.

## About

This repository contains the source code for the Foundry Local website, built with **SvelteKit** and **TailwindCSS**. The site provides:

- Product overview and features
- Interactive models catalog with search and filtering
- Links to official documentation on Microsoft Learn
- Dark mode support
- Responsive design for all devices

## Technology Stack

- **Framework**: SvelteKit 2.x
- **Styling**: TailwindCSS with custom components from shadcn-svelte
- **Language**: TypeScript
- **Build Tool**: Vite 6.x
- **Deployment**: Vercel (via adapter-auto)

## Development

### Prerequisites

- Node.js 18+ or 20+
- npm or pnpm

### Setup

```bash
# Install dependencies
npm install

# Start development server
npm run dev

# Open http://localhost:5173
```

### Available Scripts

- `npm run dev` - Start development server
- `npm run build` - Build for production
- `npm run preview` - Preview production build
- `npm run check` - Run Svelte type checking
- `npm run format` - Format code with Prettier
- `npm run lint` - Check code formatting

## Project Structure

```
www/
├── src/
│   ├── lib/
│   │   ├── components/     # Reusable UI components
│   │   ├── config.ts       # Site configuration
│   │   └── utils.ts        # Utility functions
│   ├── routes/
│   │   ├── +page.svelte    # Homepage
│   │   ├── +layout.svelte  # Root layout
│   │   └── models/         # Models catalog page
│   ├── app.html            # HTML template
│   └── app.css             # Global styles
├── static/                 # Static assets (images, fonts)
├── package.json
├── svelte.config.js        # SvelteKit configuration
├── tailwind.config.ts      # TailwindCSS configuration
└── vite.config.ts          # Vite configuration
```

## Building for Production

```bash
# Create production build
npm run build

# Test the production build locally
npm run preview
```

The built files will be in `.svelte-kit/output/`.

## Deployment

This site is configured to deploy automatically to Vercel using `@sveltejs/adapter-auto`. Push to the main branch to trigger a deployment.

### Environment Variables

No environment variables are required for basic functionality. The site fetches model data from a public Azure Function endpoint.

## About Foundry Local

Foundry Local enables you to run AI models locally on your device with:

- **On-Device Inference** - All processing stays on your hardware
- **Complete Privacy** - No data sent to external services
- **OpenAI-Compatible API** - Easy integration with existing tools
- **Hardware Acceleration** - Support for NPU, GPU, and CPU

For more information, visit the [official documentation](https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started).

## License

MIT License - Copyright (c) Microsoft Corporation

## Support

- **Documentation**: [Microsoft Learn](https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started)
- **Issues**: [GitHub Issues](https://github.com/microsoft/foundry-local/issues)
- **Community**: [GitHub Discussions](https://github.com/microsoft/foundry-local/discussions)
