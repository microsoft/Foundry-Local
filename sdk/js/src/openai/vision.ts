// -------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// -------------------------------------------------------------------------

import * as path from 'path';
import { promises as fsPromises } from 'fs';
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
     * (preserving aspect ratio) before encoding. Must be a finite positive integer.
     * Requires the `sharp` package to be installed as an optional peer dependency
     * (`npm install sharp`). If `sharp` is not available, a warning is printed and
     * the original image is used unresized.
     */
    maxDimension?: number;
}

/**
 * Creates an `InputImageContent` part by reading an image file from disk.
 * The file is base64-encoded and embedded directly in the content part.
 *
 * The second argument accepts either an `ImageContentOptions` object or a shorthand
 * detail string (`'low' | 'high' | 'auto'`) for convenience.
 *
 * @param filePath - Absolute or relative path to the image file.
 * @param options - Optional `ImageContentOptions`, or a shorthand detail string.
 * @returns A `Promise<InputImageContent>` with base64-encoded image data.
 * @throws If the file does not exist, the extension is unsupported, or `maxDimension`
 *         is not a finite positive integer.
 */
export async function createImageContentFromFile(
    filePath: string,
    options?: ImageContentOptions | 'low' | 'high' | 'auto'
): Promise<InputImageContent> {
    // Support the shorthand signature: createImageContentFromFile(path, detail?)
    const opts: ImageContentOptions = typeof options === 'string'
        ? { detail: options }
        : (options ?? {});

    if (opts.maxDimension !== undefined) {
        if (!Number.isFinite(opts.maxDimension) || !Number.isInteger(opts.maxDimension) || opts.maxDimension <= 0) {
            throw new Error(`Invalid maxDimension: ${opts.maxDimension}. Expected a finite positive integer.`);
        }
    }

    const ext = path.extname(filePath).toLowerCase();
    const mediaType = MEDIA_TYPE_MAP[ext];
    if (!mediaType) {
        throw new Error(
            `Unsupported image format: ${ext}. Supported formats: ${Object.keys(MEDIA_TYPE_MAP).join(', ')}`
        );
    }

    let dataBuffer: Buffer;
    try {
        dataBuffer = await fsPromises.readFile(filePath) as Buffer;
    } catch (err: any) {
        if (err.code === 'ENOENT') {
            throw new Error(`Image file not found: ${filePath}`);
        }
        throw err;
    }

    let finalMediaType = mediaType;
    if (opts.maxDimension !== undefined) {
        const resized = await resizeImage(dataBuffer, opts.maxDimension, mediaType);
        dataBuffer = resized.buffer;
        finalMediaType = resized.mediaType;
    }

    const content: InputImageContent = {
        type: 'input_image',
        image_data: dataBuffer.toString('base64'),
        media_type: finalMediaType,
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
 * Returns both the (possibly resized) buffer and the media type.
 */
async function resizeImage(data: Buffer, maxDimension: number, fallbackMediaType: string): Promise<{ buffer: Buffer; mediaType: string }> {
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
        return { buffer: data, mediaType: fallbackMediaType };
    }

    const metadata = await sharp(data).metadata();
    const { width = 0, height = 0, format } = metadata;
    // Map sharp format names back to MIME types; fall back to the original type
    const formatToMime: Record<string, string> = {
        png: 'image/png', jpeg: 'image/jpeg', gif: 'image/gif',
        webp: 'image/webp', bmp: 'image/bmp',
    };
    const mediaType = (format && formatToMime[format]) ?? fallbackMediaType;

    if (Math.max(width, height) <= maxDimension) {
        return { buffer: data, mediaType };
    }

    const resizedBuffer: Buffer = await sharp(data)
        .resize({ width: maxDimension, height: maxDimension, fit: 'inside', withoutEnlargement: true })
        .toBuffer();

    return { buffer: resizedBuffer, mediaType };
}
