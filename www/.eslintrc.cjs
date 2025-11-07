module.exports = {
	root: true,
	extends: [
		'eslint:recommended',
		'plugin:@typescript-eslint/recommended',
		'plugin:svelte/recommended',
		'prettier'
	],
	parser: '@typescript-eslint/parser',
	plugins: ['@typescript-eslint'],
	parserOptions: {
		sourceType: 'module',
		ecmaVersion: 2020,
		extraFileExtensions: ['.svelte']
	},
	env: {
		browser: true,
		es2017: true,
		node: true
	},
	overrides: [
		{
			files: ['*.svelte'],
			parser: 'svelte-eslint-parser',
			parserOptions: {
				parser: '@typescript-eslint/parser'
			}
		}
	],
	rules: {
		// TypeScript-specific rules
		'@typescript-eslint/no-explicit-any': 'error',
		'@typescript-eslint/explicit-function-return-type': 'off',
		'@typescript-eslint/no-unused-vars': [
			'error',
			{
				argsIgnorePattern: '^_',
				varsIgnorePattern: '^_'
			}
		],

		// General code quality
		'no-console': [
			'warn',
			{
				allow: ['warn', 'error']
			}
		],
		'no-debugger': 'error',
		'prefer-const': 'error',
		'no-var': 'error',

		// Svelte-specific overrides
		'svelte/valid-compile': 'error',
		'svelte/no-at-html-tags': 'warn'
	},
	ignorePatterns: [
		'.svelte-kit/**',
		'build/**',
		'node_modules/**',
		'*.cjs',
		'vite.config.ts',
		'svelte.config.js'
	]
};
