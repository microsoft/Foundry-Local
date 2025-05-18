// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

'use strict'

module.exports = {
  root: true,
  ignorePatterns: ['node_modules/', 'dist/'],
  env: {
    commonjs: true,
    es2021: true,
    node: true,
  },
  parser: '@typescript-eslint/parser',
  parserOptions: {
    ecmaVersion: 'latest',
    sourceType: 'module',
    project: './tsconfig.json',
  },
  plugins: ['@typescript-eslint', 'header', 'import', 'jsdoc'],
  extends: [
    'eslint:recommended',
    'plugin:@typescript-eslint/eslint-recommended',
    'plugin:@typescript-eslint/recommended',
  ],
  rules: {
    'header/header': [
      2,
      'line',
      [' Copyright (c) Microsoft Corporation. All rights reserved.', ' Licensed under the MIT License.'],
      2,
    ],
    'import/no-extraneous-dependencies': ['error', { devDependencies: false }],
    'import/no-unassigned-import': 'error',
    'jsdoc/check-alignment': 'error',
    'jsdoc/check-indentation': 'error',
    '@typescript-eslint/await-thenable': 'error',
    camelcase: 'error',
    curly: 'error',
    'no-debugger': 'error',
    'no-unused-vars': 'off',
  },
}
