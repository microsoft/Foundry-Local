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
    '.bmp': 'image/bmp',
};

/**
 * Options for `createImageContentFromFile`.
 */
export interface ImageContentOptions {
    /** Detail level hint for the model. */
    detail?: 'low' | 'high' | 'auto';
    /**
     * If set, the longest dimension of the image will be scaled down to this value
     * (preserving aspect ratio) before encoding. Requires the `sharp` package to be
     * installed as an optional peer dependency (`npm install sharp`). If `sharp` is
     * not available and the image exceeds this size, a warning is printed and the
     * original image is used unresized.
     */
    maxDimension?: number;
}

/**
 * Creates an `InputImageContent` part by reading an image file from disk.
 * The file is base64-encoded and embedded directly in the content part.
 *
 * @param filePath - Absolute or relative path to the image file.
 * @param options - Optional settings (detail level, max dimension for resize).
 * @returns An `InputImageContent` object with base64-encoded image data.
 * @throws If the file does not exist or the extension is not a supported format.
 */
export async function createImageContentFromFile(
    filePath: string,
    options?: ImageContentOptions | 'low' | 'high' | 'auto'
): Promise<InputImageContent> {
    // Support the original simple signature: createImageContentFromFile(path, detail?)
    const opts: ImageContentOptions = typeof options === 'string'
        ? { detail: options }
        : (options ?? {});

    if (!fs.existsSync(filePath)) {
        throw new Error(`Image file not found: ${filePath}`);
    }

    const ext = path.extname(filePath).toLowerCase();
    const mediaType = MEDIA_TYPE_MAP[ext];
    if (!mediaType) {
        throw new Error(
            `Unsupported image format: ${ext}. Supported formats: ${Object.keys(MEDIA_TYPE_MAP).join(', ')}`
        );
    }

    let dataBuffer: Buffer = fs.readFileSync(filePath);

    if (opts.maxDimension !== undefined) {
        dataBuffer = await resizeImage(dataBuffer, opts.maxDimension, filePath);
    }

    const content: InputImageContent = {
        type: 'input_image',
        image_data: dataBuffer.toString('base64'),
        media_type: mediaType,
    };
    if (opts.detail !== undefined) {
        content.detail = opts.detail;
    }
    return content;
}

/**
 * Creates an `InputImageContent` part from a URL.
 * The server will infer the media type from the URL.
 *
 * @param url - Public URL pointing to the image.
 * @param detail - Optional detail level hint for the model ('low' | 'high' | 'auto').
 * @returns An `InputImageContent` object with the image URL.
 */
export function createImageContentFromUrl(url: string, detail?: 'low' | 'high' | 'auto'): InputImageContent {
    const content: InputImageContent = {
        type: 'input_image',
        image_url: url,
        // media_type intentionally omitted — server infers from URL
    };
    if (detail !== undefined) {
        content.detail = detail;
    }
    return content;
}

/**
 * Attempts to resize image data to fit within `maxDimension` on the longest side.
 * Requires the optional `sharp` peer dependency. Falls back to original data with a
 * warning if `sharp` is not available.
 */
async function resizeImage(data: Buffer, maxDimension: number, filePath: string): Promise<Buffer> {
    let sharp: any;
    try {
        // Dynamic import so sharp remains a soft/optional peer dep.
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-ignore — sharp is an optional peer dependency
        sharp = (await import('sharp')).default;
    } catch {
        console.warn(
            `[foundry-local] createImageContentFromFile: maxDimension=${maxDimension} requires the ` +
            `"sharp" package (npm install sharp). Image will be used unresized.`
        );
        return data;
    }

    const metadata = await sharp(data).metadata();
    const { width = 0, height = 0 } = metadata;

    if (Math.max(width, height) <= maxDimension) {
        return data; // already within bounds
    }

    const resized: Buffer = await sharp(data)
        .resize({ width: maxDimension, height: maxDimension, fit: 'inside', withoutEnlargement: true })
        .toBuffer();

    return resized;
}
