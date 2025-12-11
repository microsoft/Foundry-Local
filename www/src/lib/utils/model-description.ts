// Template-based dynamic model description generator
// Replaces static API descriptions with context-aware text based on device/framework

import type { GroupedFoundryModel } from '../../routes/models/types';

/**
 * Acceleration display names mapping
 */
const ACCELERATION_DISPLAY_NAMES: Record<string, string> = {
    qnn: 'Qualcomm AI Engine',
    vitis: 'AMD Vitis AI',
    openvino: 'Intel OpenVINO',
    cuda: 'NVIDIA CUDA',
    'trt-rtx': 'NVIDIA TensorRT',
    trtrtx: 'NVIDIA TensorRT',
    webgpu: 'WebGPU'
};

/**
 * Format a list of devices into a human-readable string
 * e.g., ['cpu', 'gpu', 'npu'] -> "CPU, GPU, and NPU"
 */
function formatDeviceList(devices: string[]): string {
    if (devices.length === 0) return 'various devices';

    const formatted = devices.map((d) => d.toUpperCase());

    if (formatted.length === 1) {
        return formatted[0];
    } else if (formatted.length === 2) {
        return `${formatted[0]} and ${formatted[1]}`;
    } else {
        const last = formatted.pop();
        return `${formatted.join(', ')}, and ${last}`;
    }
}

/**
 * Get the display name for an acceleration type
 */
function getAccelerationDisplayName(acceleration: string): string {
    return ACCELERATION_DISPLAY_NAMES[acceleration.toLowerCase()] || acceleration;
}

/**
 * Get unique accelerations from a grouped model's variants
 */
function getUniqueAccelerations(model: GroupedFoundryModel): string[] {
    if (!model.variants || model.variants.length === 0) {
        return model.acceleration ? [model.acceleration] : [];
    }

    const accelerations = new Set<string>();
    for (const variant of model.variants) {
        if (variant.acceleration) {
            accelerations.add(variant.acceleration);
        }
    }
    return Array.from(accelerations);
}

/**
 * Generate a dynamic description for a model based on its properties
 */
export function generateModelDescription(model: GroupedFoundryModel): string {
    const displayName = model.displayName || model.alias || 'this model';
    const devices = model.deviceSupport || [];
    const accelerations = getUniqueAccelerations(model);

    // Base description
    let description = `${displayName} is optimized for local inference`;

    // Add device information
    if (devices.length > 0) {
        description += ` on ${formatDeviceList(devices)}`;
    }

    description += '.';

    // Add acceleration framework information
    if (accelerations.length > 0) {
        const accelerationNames = accelerations.map(getAccelerationDisplayName);
        if (accelerationNames.length === 1) {
            description += ` Uses ${accelerationNames[0]} for acceleration.`;
        } else if (accelerationNames.length === 2) {
            description += ` Available with ${accelerationNames[0]} or ${accelerationNames[1]} acceleration.`;
        } else {
            const last = accelerationNames.pop();
            description += ` Available with ${accelerationNames.join(', ')}, or ${last} acceleration.`;
        }
    }

    return description;
}
