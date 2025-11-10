/**
 * Detect the user's operating system platform
 */
export function detectPlatform(): 'windows' | 'macos' | 'linux' | 'unknown' {
	if (typeof navigator === 'undefined' || typeof window === 'undefined') {
		return 'unknown';
	}

	const userAgent = navigator.userAgent.toLowerCase();
	const platform = navigator.platform?.toLowerCase() || '';

	// Check for macOS
	if (platform.includes('mac') || userAgent.includes('mac')) {
		return 'macos';
	}

	// Check for Windows
	if (platform.includes('win') || userAgent.includes('win')) {
		return 'windows';
	}

	// Check for Linux
	if (platform.includes('linux') || userAgent.includes('linux')) {
		return 'linux';
	}

	return 'unknown';
}

/**
 * Get the installation command for the detected platform
 */
export function getInstallCommand(platform?: 'windows' | 'macos' | 'linux' | 'unknown'): {
	command: string;
	label: string;
	description: string;
} {
	const detectedPlatform = platform || detectPlatform();

	switch (detectedPlatform) {
		case 'windows':
			return {
				command: 'winget install Microsoft.FoundryLocal',
				label: 'Windows',
				description: 'Install via Windows Package Manager'
			};
		case 'macos':
			return {
				command: 'brew install microsoft/foundrylocal/foundrylocal',
				label: 'macOS',
				description: 'Install via Homebrew'
			};
		case 'linux':
		case 'unknown':
		default:
			// Default to brew for Linux/unknown as it's available on Linux too
			return {
				command: 'brew install microsoft/foundrylocal/foundrylocal',
				label: 'Install',
				description: 'Install via Homebrew'
			};
	}
}
