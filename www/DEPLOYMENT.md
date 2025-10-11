# Foundry Local Website Deployment Guide

This document provides instructions for deploying the Foundry Local website to production.

## Pre-Deployment Checklist

Before deploying, ensure all of the following items are completed:

### Code Quality

- [ ] All console.log statements removed from production code
- [ ] Code formatted with Prettier (`npm run format`)
- [ ] No linting errors (`npm run lint`)
- [ ] TypeScript compilation successful (`npm run check`)
- [ ] Production build successful (`npm run build`)

### Content

- [ ] All user-facing text proofread for typos
- [ ] External links verified and working
- [ ] Copyright year is current (2025)
- [ ] Package.json metadata is accurate
- [ ] README.md is up-to-date

### SEO & Meta Tags

- [ ] Open Graph tags configured in `src/app.html`
- [ ] Twitter Card tags configured
- [ ] `robots.txt` present in `/static`
- [ ] `sitemap.xml` present in `/static`
- [ ] `manifest.json` configured for PWA

### Assets

- [ ] All images optimized
- [ ] Favicon present and correct
- [ ] Images have alt text for accessibility
- [ ] Large bundle warnings addressed

### Security

- [ ] Security headers configured in `vercel.json`
- [ ] No secrets or API keys in client code
- [ ] External links use `rel="noopener"` when appropriate

## Environment Variables

**No environment variables are required** for this deployment. The site fetches model data from a public Azure Function endpoint.

## Deployment to Vercel

### Option 1: Automatic Deployment (Recommended)

1. **Connect Repository to Vercel**
   - Visit [vercel.com](https://vercel.com)
   - Click "New Project"
   - Import your GitHub repository
   - Vercel will auto-detect SvelteKit

2. **Configure Project Settings**

   ```
   Framework Preset: SvelteKit
   Build Command: npm run build
   Output Directory: .svelte-kit/output
   Install Command: npm install
   ```

3. **Deploy**
   - Click "Deploy"
   - Subsequent pushes to `main` branch will auto-deploy

### Option 2: Manual Deployment

```bash
# Install Vercel CLI
npm i -g vercel

# Login to Vercel
vercel login

# Deploy to production
vercel --prod
```

## Post-Deployment Verification

After deployment, verify the following:

### Functionality

- [ ] Homepage loads correctly
- [ ] Models page loads and displays models
- [ ] Search functionality works
- [ ] Filter controls work (NPU, GPU, CPU)
- [ ] Dark mode toggle works
- [ ] Copy buttons work (install command, model IDs)
- [ ] All navigation links work
- [ ] External links open in new tabs

### Performance

- [ ] Page loads in < 3 seconds
- [ ] No console errors in browser
- [ ] Images load properly
- [ ] Mobile responsive design works

### SEO

- [ ] Meta tags visible in page source
- [ ] `robots.txt` accessible at `/robots.txt`
- [ ] `sitemap.xml` accessible at `/sitemap.xml`
- [ ] Structured data validates (use [Schema.org validator](https://validator.schema.org/))

### Social Media

- [ ] Open Graph preview works (use [OpenGraph.xyz](https://www.opengraph.xyz/))
- [ ] Twitter Card preview works (use [Twitter Card Validator](https://cards-dev.twitter.com/validator))

## Rollback Procedures

### Via Vercel Dashboard

1. Go to your project in Vercel
2. Click "Deployments" tab
3. Find the previous working deployment
4. Click the three dots menu
5. Select "Promote to Production"

### Via Vercel CLI

```bash
# List recent deployments
vercel ls

# Promote a specific deployment
vercel promote <deployment-url>
```

## Monitoring & Maintenance

### Analytics

Vercel provides built-in analytics at:

- **Dashboard**: `https://vercel.com/<team>/<project>/analytics`
- Metrics include: page views, unique visitors, top pages

### Error Monitoring

Check Vercel logs for errors:

```bash
vercel logs <deployment-url>
```

### Regular Maintenance

- **Weekly**: Check analytics for traffic patterns
- **Monthly**: Update dependencies (`npm update`)
- **Quarterly**: Review and optimize bundle sizes
- **As needed**: Update content and model catalog

## Troubleshooting

### Build Fails

```bash
# Clear cache and rebuild locally
rm -rf .svelte-kit node_modules
npm install
npm run build
```

### 404 Errors on Routes

- Check that `@sveltejs/adapter-auto` is properly configured
- Verify routes are prerendered or have proper server logic
- Check Vercel logs for routing issues

### Performance Issues

1. Check bundle size warnings in build output
2. Consider code splitting with dynamic imports
3. Optimize images (convert to WebP)
4. Review Tailwind CSS purging

### API Request Failures

The models page fetches from:

```
https://onnxruntime-foundry-proxy-hpape7gzf2haesef.eastus-01.azurewebsites.net/api/foundryproxy
```

If this fails:

- Check Azure Function is running
- Verify CORS configuration
- Check network/firewall issues

## Support & Contact

For deployment issues:

- **Vercel Support**: https://vercel.com/support
- **GitHub Issues**: https://github.com/microsoft/foundry-local/issues
- **Internal Team**: Contact the website team

---

**Last Updated**: October 9, 2025  
**Maintained By**: Foundry Local Team
