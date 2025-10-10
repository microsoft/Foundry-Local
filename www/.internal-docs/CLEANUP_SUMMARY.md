# Foundry Local Website Cleanup Summary

This document summarizes all changes made to align the Foundry Local website with the actual product capabilities and redirect documentation to Microsoft Learn.

## Date

October 9, 2025

## Overview

Performed a comprehensive cleanup of marketing claims and documentation links to ensure accuracy and alignment with Foundry Local's actual capabilities as documented in `/docs`.

---

## Files Modified

### 1. `/src/lib/components/home/features.svelte`

**Changes:**

- **Data Privacy Section (Lines 46-93)**
  - ❌ Removed: "HIPAA, GDPR, and PCI compliant deployments"
  - ❌ Removed: "Complete audit logging and monitoring"
  - ✅ Added: "All processing stays on your device"
  - ✅ Added: "Works in air-gapped environments"
  - ✅ Added: "No telemetry or usage data collection"
  - Updated description to clarify "No data is sent to Microsoft or any external services"
  - Changed link from `/docs/security` to Microsoft Learn

- **Azure Compatibility Section (Lines 95-134)**
  - Changed title from "Seamless Cloud Transition" to "OpenAI-Compatible API"
  - ❌ Removed: "One-click deployment to Azure"
  - ❌ Removed: "Compatible Azure AI REST API endpoints"
  - ✅ Changed focus to: "OpenAI-compatible REST API"
  - ✅ Added: "Python and JavaScript SDKs"
  - ✅ Added: "Compatible with popular AI frameworks"
  - Updated description to focus on API compatibility rather than cloud transition
  - Changed link from `/docs/azure-compatibility` to Microsoft Learn

**Rationale:** The service provides local AI inference with data privacy. Claims about compliance certifications, audit logging, and one-click Azure deployment were not supported by documentation.

---

### 2. `/src/lib/components/home/hero.svelte`

**Changes:**

- **Line 10:** Changed install command
  - ❌ Before: `curl -sSL https://get.foundrylocal.ai | bash` (placeholder)
  - ✅ After: `winget install Microsoft.FoundryLocal` (actual command)

- **Line 83:** Updated "Get Started" button link
  - ❌ Before: `/docs`
  - ✅ After: `https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started`

**Rationale:** The curl command was a placeholder that doesn't exist. The actual installation methods are winget (Windows) and brew (macOS).

---

### 3. `/src/lib/config.ts`

**Changes:**

- **Site Description (Lines 18-19)**
  - ❌ Before: "Run AI models locally with the power of Microsoft's Azure AI technology. Foundry Local provides a secure, private environment for AI development and inference."
  - ✅ After: "Run AI models locally on your device. Foundry Local provides on-device inference with complete data privacy, no Azure subscription required."

- **Quick Links (Lines 22-25)**
  - Changed "Getting Started" from `/docs` to Microsoft Learn URL
  - Changed "Model Hub" to "GitHub" link

- **Nav Items (Lines 31-42)**
  - Changed "Documentation" link from `/docs` to Microsoft Learn URL

- **Features Array (Lines 57-78)**
  - **Feature 1:** "Local Deployment" → "On-Device Inference"
    - New description emphasizes no cloud dependencies or Azure subscription
  - **Feature 2:** "Model Integration" → "Optimized Performance"
    - New description focuses on ONNX Runtime and hardware acceleration
  - **Feature 3:** "Azure Compatible" → "OpenAI-Compatible API"
    - New description focuses on OpenAI SDK compatibility
  - **Feature 4:** "Developer-First" → "Python & JavaScript SDKs"
    - New description emphasizes available SDKs

- **Promo Config (Lines 80-88)**
  - ❌ Before: "Scale to the cloud when ready" / "seamlessly transition"
  - ✅ After: "Need to scale to the cloud?" / "enterprise-scale AI infrastructure when your project outgrows local deployment"

**Rationale:** Simplified messaging to focus on actual product capabilities: local execution, ONNX optimization, OpenAI compatibility, and available SDKs.

---

### 4. `/src/lib/components/home/footer.svelte`

**Changes:**

- **Product Links (Lines 19-36)**
  - Documentation: `/docs` → Microsoft Learn URL
  - Model Hub: `/docs/models` → `/models` (internal page kept)
  - Configuration: `/docs/configuration` → "GitHub Repository" (github.com/microsoft/foundry-local)
  - Releases: Kept as-is (GitHub releases page)

- **Resources Links (Lines 38-56)**
  - Examples: `/docs/examples` → GitHub samples directory
  - GitHub: Duplicate removed, replaced with "Support" → GitHub issues

**Rationale:** Removed broken internal docs links, redirected to Microsoft Learn and appropriate GitHub pages.

---

### 5. New Files Created

#### `/DOCS_INFRASTRUCTURE_README.md`

**Purpose:** Comprehensive documentation of the docs system architecture

**Contents:**

- Overview of the markdown-based docs system
- Directory structure and file organization
- How route loading and navigation work
- Component descriptions (doc-renderer, table-of-contents, sidebar, etc.)
- Step-by-step instructions for reactivating the docs system
- Current state and what was changed
- Dependencies and configuration files

**Rationale:** Preserve institutional knowledge about the docs infrastructure for future reactivation if needed.

#### `/CLEANUP_SUMMARY.md` (this file)

**Purpose:** Document all changes made during this cleanup

---

## What Was NOT Changed

### Files/Systems Preserved

1. **Docs Routes** (`/src/routes/docs/`)
   - All route files preserved
   - Can still be accessed directly via URL
   - Just not linked from main navigation

2. **Docs Components** (`/src/lib/components/document/`)
   - All doc-specific components intact
   - doc-renderer, table-of-contents, etc.
   - Can be reactivated without code changes

3. **Content Files** (`/src/content/`)
   - All markdown documentation files preserved
   - Ready for future use

4. **Models Page** (`/src/routes/models/`)
   - Kept as-is, still accessible
   - Linked from footer

5. **Promo Card Component**
   - Component intact
   - Only config text updated

---

## Claims Removed

### Overselling / Unsubstantiated

1. ❌ "HIPAA, GDPR, and PCI compliant deployments"
   - Not documented anywhere in `/docs`
   - Service just runs offline, doesn't guarantee compliance

2. ❌ "Complete audit logging and monitoring"
   - No evidence of this feature in documentation
   - Docs mention "No telemetry"

3. ❌ "One-click deployment to Azure"
   - Not a documented feature
   - No tooling for this exists

4. ❌ "Compatible Azure AI REST API endpoints"
   - Misleading phrasing
   - Actually: "OpenAI-compatible API"

---

## New/Accurate Claims

### Added/Updated

1. ✅ "All processing stays on your device"
   - Directly from security docs

2. ✅ "Works in air-gapped environments"
   - Documented in security reference

3. ✅ "No telemetry or usage data collection"
   - Explicitly stated in security docs

4. ✅ "OpenAI-compatible REST API"
   - Accurate description from README

5. ✅ "Python and JavaScript SDKs"
   - Documented and available

6. ✅ "Powered by ONNX Runtime"
   - Core technology accurately described

7. ✅ "No Azure subscription required"
   - Key differentiator, accurately stated

---

## Documentation Strategy

### Before This Cleanup

- Internal docs system built with mdsvex
- Docs routes at `/docs`
- Navigation linked to internal pages
- Attempted to be comprehensive doc site

### After This Cleanup

- Main documentation → Microsoft Learn
- Internal docs infrastructure **preserved but not linked**
- Can be reactivated if needed
- Focus website on product overview and model hub

### Microsoft Learn URL

`https://learn.microsoft.com/en-us/azure/ai-foundry/foundry-local/get-started`

---

## Testing Results

### Build Status

✅ **Build Successful**

```bash
npm run build
# Result: Success with minor warnings
# - 1 warning about unused CSS selector (cosmetic)
# - 1 warning about large chunks (performance consideration)
# - All routes built successfully
```

### Warnings (Non-Critical)

1. **Unused CSS selector `.line-clamp-1`** in `models/+page.svelte`
   - Cosmetic issue, doesn't affect functionality

2. **Large chunk warning** (622KB, 697KB chunks)
   - Performance consideration for future optimization
   - Not blocking

3. **404 Warning** for `/docs/styling`
   - Expected, internal docs not fully populated
   - Not user-facing

---

## Alignment with Actual Docs

### What Foundry Local Actually Does (from `/docs`)

✅ Runs LLMs locally on your device
✅ Uses ONNX Runtime for optimization
✅ Supports hardware acceleration (GPU, NPU)
✅ Provides OpenAI-compatible API
✅ Offers Python and JavaScript SDKs
✅ Works in air-gapped environments
✅ No telemetry collection
✅ No Azure subscription required
✅ Free to use (using your own hardware)

### What Was Being Claimed (Before Cleanup)

❌ HIPAA/GDPR/PCI compliance
❌ Audit logging and monitoring
❌ One-click Azure deployment
❌ Azure AI REST API endpoints
❌ Seamless cloud transitions

---

## Recommendations for Future

### Short Term

1. Monitor Microsoft Learn docs for updates
2. Keep internal docs infrastructure maintained
3. Consider syncing `/docs` markdown with website content
4. Update version numbers as releases happen

### Long Term

1. **Consider Dual Documentation Strategy:**
   - Microsoft Learn: User-facing guides, getting started
   - Internal Docs: API reference, advanced topics, developer content
2. **Add More Accurate Content:**
   - Performance benchmarks
   - Model comparison tables
   - Hardware requirements detail
   - Actual use cases from users

3. **Improve Model Hub:**
   - Already exists at `/models`
   - Could expand with more details
   - Filter by hardware compatibility

---

## Questions or Issues?

If you need to:

- **Reactivate internal docs:** See `/DOCS_INFRASTRUCTURE_README.md`
- **Update marketing claims:** Verify against `/docs` first
- **Add new features:** Update both website and Microsoft Learn
- **Report issues:** Use GitHub issues

---

## Summary

This cleanup ensures the Foundry Local website:

- ✅ Makes only claims supported by documentation
- ✅ Links to official Microsoft Learn documentation
- ✅ Focuses on actual product capabilities
- ✅ Preserves infrastructure for future use
- ✅ Builds successfully without errors
- ✅ Maintains professional, accurate messaging

**No functionality was removed**, only marketing claims corrected and links updated.
