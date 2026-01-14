// Device icon mapping
const DEVICE_ICONS: Record<string, string> = {
    npu: '🧠',
    gpu: '🎮',
    cpu: '💻'
};

export function getDeviceIcon(device: string): string {
    return DEVICE_ICONS[device.toLowerCase()] || '🔧';
}

// Accelerator logo patterns and paths
const ACCELERATOR_LOGO_PATTERNS: Array<{ patterns: string[]; logo: string }> = [
    { patterns: ['-cuda-', '-cuda', '-tensorrt-', '-tensorrt', '-trt-rtx-', '-trt-rtx', '-trtrtx'], logo: '/logos/nvidia-logo.svg' },
    { patterns: ['-qnn-', '-qnn'], logo: '/logos/qualcomm-logo.svg' },
    { patterns: ['-vitis-', '-vitis', '-vitisai'], logo: '/logos/amd-logo.svg' },
    { patterns: ['-openvino-', '-openvino'], logo: '/logos/intel-logo.svg' },
    { patterns: ['-webgpu-', '-webgpu', 'webgpu', '-generic-gpu'], logo: '/logos/webgpu-logo.svg' }
];

export function getAcceleratorLogo(variantName: string): string | null {
    const name = variantName.toLowerCase();
    for (const { patterns, logo } of ACCELERATOR_LOGO_PATTERNS) {
        if (patterns.some(pattern => name.includes(pattern))) {
            return logo;
        }
    }
    return null;
}

// Accelerator color patterns
const ACCELERATOR_COLOR_PATTERNS: Array<{ patterns: string[]; color: string }> = [
    { patterns: ['-cuda-', '-cuda', '-tensorrt-', '-tensorrt', '-trt-rtx-', '-trt-rtx', '-trtrtx'], color: '#76B900' },
    { patterns: ['-qnn-', '-qnn'], color: '#3253DC' },
    { patterns: ['-vitis-', '-vitis', '-vitisai'], color: 'var(--amd-color, #000000)' },
    { patterns: ['-openvino-', '-openvino'], color: '#0071C5' },
    { patterns: ['-webgpu-', '-webgpu', 'webgpu', '-generic-gpu'], color: '#005A9C' }
];

export function getAcceleratorColor(variantName: string): string {
    const name = variantName.toLowerCase();
    for (const { patterns, color } of ACCELERATOR_COLOR_PATTERNS) {
        if (patterns.some(pattern => name.includes(pattern))) {
            return color;
        }
    }
    return 'currentColor';
}

// Variant label patterns - ordered by specificity (most specific first)
const VARIANT_LABEL_PATTERNS: Array<{ patterns: string[]; label: string; requiresAll?: boolean }> = [
    { patterns: ['-cuda-', '-trt-rtx-'], label: 'CUDA + TensorRT', requiresAll: true },
    { patterns: ['-cuda-', '-tensorrt-'], label: 'CUDA + TensorRT', requiresAll: true },
    { patterns: ['-cuda-', '-trtrtx'], label: 'CUDA + TensorRT', requiresAll: true },
    { patterns: ['-cuda-gpu', '-cuda-'], label: 'CUDA' },
    { patterns: ['-generic-gpu', 'webgpu'], label: 'WebGPU' },
    { patterns: ['-qnn-'], label: 'QNN' },
    { patterns: ['-vitis-'], label: 'Vitis' },
    { patterns: ['-openvino-'], label: 'OpenVINO' },
    { patterns: ['-trt-rtx-', '-tensorrt-', '-trtrtx-', '-trtrtx'], label: 'TensorRT' },
    { patterns: ['-generic-cpu'], label: 'Generic' }
];

export function getVariantLabel(variant: { name: string; deviceSupport: string[] }): string {
    const modelName = variant.name.toLowerCase();
    const device = variant.deviceSupport[0]?.toUpperCase() || '';

    for (const { patterns, label, requiresAll } of VARIANT_LABEL_PATTERNS) {
        if (requiresAll) {
            if (patterns.every(pattern => modelName.includes(pattern))) {
                return `${device} (${label})`;
            }
        } else {
            if (patterns.some(pattern => modelName.includes(pattern))) {
                return `${device} (${label})`;
            }
        }
    }

    return device;
}

// Acceleration to logo mapping
const ACCELERATION_LOGOS: Record<string, string> = {
    cuda: '/logos/nvidia-logo.svg',
    'trt-rtx': '/logos/nvidia-logo.svg',
    trtrtx: '/logos/nvidia-logo.svg',
    qnn: '/logos/qualcomm-logo.svg',
    vitis: '/logos/amd-logo.svg',
    openvino: '/logos/intel-logo.svg',
    webgpu: '/logos/webgpu-logo.svg'
};

export function getAcceleratorLogoFromAcceleration(acceleration: string): string | null {
    return ACCELERATION_LOGOS[acceleration.toLowerCase()] || null;
}

// Acceleration to color mapping
const ACCELERATION_COLORS: Record<string, string> = {
    cuda: '#76B900',
    'trt-rtx': '#76B900',
    trtrtx: '#76B900',
    qnn: '#3253DC',
    vitis: 'var(--amd-color, #000000)',
    openvino: '#0071C5',
    webgpu: '#005A9C'
};

export function getAcceleratorColorFromAcceleration(acceleration: string): string {
    return ACCELERATION_COLORS[acceleration.toLowerCase()] || 'currentColor';
}
