---
title: UI Customization
description: Learn how to customize the Foundry Local interface and appearance
---

# UI Customization

This guide explains how to customize the visual appearance of your Foundry Local installation to match your organization's branding or personal preferences.

## Web Interface Themes

Foundry Local's web interface supports comprehensive theming to align with your organization's branding.

### Default Themes

Foundry Local comes with several pre-built themes:

1. **Microsoft Standard** (default) - Aligned with Microsoft's design language
2. **Azure AI** - Matches the Azure AI service styling
3. **Dark Pro** - High-contrast dark theme optimized for low-light environments
4. **Light Minimal** - Clean, minimal interface with lighter colors
5. **High Contrast** - Accessibility-focused theme with maximum contrast

To change themes through the web interface:

1. Navigate to **Settings > Appearance**
2. Select your preferred theme from the dropdown menu
3. Click **Apply Theme** to save changes

### Custom Theme Configuration

For more advanced customization, create a custom theme JSON file:

```json
{
	"name": "Organization Brand",
	"colors": {
		"background": "#ffffff",
		"foreground": "#1a1a1a",
		"primary": "#0078d4",
		"primaryForeground": "#ffffff",
		"secondary": "#f3f3f3",
		"secondaryForeground": "#1a1a1a",
		"accent": "#e6f2fa",
		"accentForeground": "#0067b5",
		"success": "#107c10",
		"warning": "#ff8c00",
		"error": "#d13438",
		"surface": "#fafafa",
		"sidebar": "#f3f3f3",
		"sidebarForeground": "#1a1a1a"
	},
	"typography": {
		"fontFamily": "'Segoe UI', system-ui, sans-serif",
		"codeFamily": "'Cascadia Code', monospace",
		"baseFontSize": "16px",
		"lineHeight": "1.5"
	},
	"radius": "4px",
	"shadows": {
		"sm": "0 1px 2px rgba(0,0,0,0.05)",
		"md": "0 4px 6px rgba(0,0,0,0.1)",
		"lg": "0 10px 15px rgba(0,0,0,0.1)"
	}
}
```

Save this file as `custom-theme.json` and apply it:

```bash
foundry-local theme import ./custom-theme.json
```

### Dark Mode Configuration

Foundry Local automatically supports dark mode based on system preferences. Customize dark mode options:

```yaml
# config.yaml
ui:
  theme:
    dark_mode:
      respect_system_preference: true
      default: auto # Options: light, dark, auto
      toggle_enabled: true
```

## Logo Customization

Replace the default Foundry Local branding with your organization's logo:

1. Prepare your logo in both light and dark versions (SVG format recommended)
2. Place the files in the Foundry Local configuration directory:
   ```bash
   cp company-logo-light.svg ~/.foundry-local/ui/logo-light.svg
   cp company-logo-dark.svg ~/.foundry-local/ui/logo-dark.svg
   ```
3. Update the configuration:
   ```yaml
   # config.yaml
   ui:
     branding:
       logo_light: '~/.foundry-local/ui/logo-light.svg'
       logo_dark: '~/.foundry-local/ui/logo-dark.svg'
       app_name: 'My Organization AI'
   ```

## Dashboard Customization

Customize the dashboard layout to highlight the most important features for your users:

```yaml
# config.yaml
ui:
  dashboard:
    layout: 'grid' # Options: grid, list, compact
    widgets:
      - type: 'model-list'
        position: 0
        expanded: true
      - type: 'system-stats'
        position: 1
      - type: 'recent-inferences'
        position: 2
        limit: 5
      - type: 'documentation'
        position: 3
        visible: false
```

## Terminal and CLI Theme

Customize the appearance of the Foundry Local command-line interface:

```yaml
# config.yaml
cli:
  theme:
    primary_color: 'blue' # Options: blue, green, purple, red, yellow
    show_animations: true
    unicode_support: true # For terminals that support Unicode characters
    compact_output: false
```

## Language and Localization

Configure the interface language and localization settings:

```yaml
# config.yaml
ui:
  localization:
    language: 'en-US' # Options: en-US, es-ES, fr-FR, de-DE, ja-JP, zh-CN
    date_format: 'MM/DD/YYYY'
    time_format: '12h' # Options: 12h, 24h
    number_format:
      decimal_separator: '.'
      thousands_separator: ','
```

## Accessibility Features

Enhance accessibility for all users with these options:

```yaml
# config.yaml
ui:
  accessibility:
    high_contrast_mode: false
    reduced_motion: false
    increased_font_size: 0 # 0-5 scale, 0 is default
    keyboard_navigation_enhanced: true
```

## Best Practices

1. **Brand Consistency**: Keep your color scheme consistent with your organization's branding guidelines
2. **Dark Mode Testing**: Always test customizations in both light and dark modes
3. **Performance**: Complex UI customizations may impact performance on lower-end devices
4. **Accessibility**: Ensure sufficient color contrast (minimum 4.5:1 for text)
5. **Organization**: Consider creating a dedicated UI customization package to deploy across multiple installations

Remember to restart Foundry Local after making UI configuration changes for them to take effect.
