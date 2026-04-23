// -------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// -------------------------------------------------------------------------

import * as fs from 'fs';
import * as path from 'path';
import type { InputImageContent } from '../types.js';

const MEDIA_TYPE_MAP: Record<string, string> = {
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.jpeg': 'image/jpeg',
    '.gif': 'image/gif',
    '.webp': 'image/webp',
};

/**
 * Creates an `InputImageContent` part by reading an image file from disk.
 * The file is base64-encoded and embedded directly in the content part.
 *
 * @param filePath - Absolute or relative path to the image file.
 * @param detail - Optional detail level hint for the model ('low' | 'high' | 'auto').
 * @returns An `InputImageContent` object with base64-encoded image data.
 * @throws If the file extension is not a supported image format.
 */
export function createImageContentFromFile(filePath: string, detail?: 'low' | 'high' | 'auto'): InputImageContent {
    const ext = path.extname(filePath).toLowerCase();
    const mediaType = MEDIA_TYPE_MAP[ext];
    if (!mediaType) {
        throw new Error(`Unsupported image format: ${ext}. Supported formats: ${Object.keys(MEDIA_TYPE_MAP).join(', ')}`);
    }

    const data = fs.readFileSync(filePath);
    const content: InputImageContent = {
        type: 'input_image',
        image_data: data.toString('base64'),
        media_type: mediaType,
    };
    if (detail !== undefined) {
        content.detail = detail;
    }
    return content;
}

/**
 * Creates an `InputImageContent` part from a URL.
 *
 * @param url - Public URL pointing to the image.
 * @param detail - Optional detail level hint for the model ('low' | 'high' | 'auto').
 * @returns An `InputImageContent` object with the image URL.
 */
export function createImageContentFromUrl(url: string, detail?: 'low' | 'high' | 'auto'): InputImageContent {
    const content: InputImageContent = {
        type: 'input_image',
        image_url: url,
        media_type: 'image/unknown', // server will detect from URL
    };
    if (detail !== undefined) {
        content.detail = detail;
    }
    return content;
}
