export function getDeviceIcon(device: string): string {
    const icons: Record<string, string> = {
        npu: '🧠',
        gpu: '🎮',
        cpu: '💻'
    };
    return icons[device.toLowerCase()] || '🔧';
}

export function getAcceleratorLogo(variantName: string): string | null {
    const name = variantName.toLowerCase();

    // NVIDIA (CUDA, TensorRT)
    if (
        name.includes('-cuda-') ||
        name.includes('-cuda') ||
        name.includes('-tensorrt-') ||
        name.includes('-tensorrt') ||
        name.includes('-trt-rtx-') ||
        name.includes('-trt-rtx') ||
        name.includes('-trtrtx')
    ) {
        return '/logos/nvidia-logo.svg';
    }

    // Qualcomm (QNN)
    if (name.includes('-qnn-') || name.includes('-qnn')) {
        return '/logos/qualcomm-logo.svg';
    }

    // AMD (Vitis)
    if (name.includes('-vitis-') || name.includes('-vitis') || name.includes('-vitisai')) {
        return '/logos/amd-logo.svg';
    }

    // Intel (OpenVINO)
    if (name.includes('-openvino-') || name.includes('-openvino')) {
        return '/logos/intel-logo.svg';
    }

    // WebGPU (check for webgpu OR generic-gpu)
    if (
        name.includes('-webgpu-') ||
        name.includes('-webgpu') ||
        name.includes('webgpu') ||
        name.includes('-generic-gpu')
    ) {
        return '/logos/webgpu-logo.svg';
    }

    return null;
}

export function getAcceleratorColor(variantName: string): string {
    const name = variantName.toLowerCase();

    if (
        name.includes('-cuda-') ||
        name.includes('-cuda') ||
        name.includes('-tensorrt-') ||
        name.includes('-tensorrt') ||
        name.includes('-trt-rtx-') ||
        name.includes('-trt-rtx') ||
        name.includes('-trtrtx')
    ) {
        return '#76B900';
    }
    if (name.includes('-qnn-') || name.includes('-qnn')) {
        return '#3253DC';
    }
    if (name.includes('-vitis-') || name.includes('-vitis') || name.includes('-vitisai')) {
        return 'var(--amd-color, #000000)'; // Black in light mode, white in dark mode
    }
    if (name.includes('-openvino-') || name.includes('-openvino')) {
        return '#0071C5';
    }
    if (
        name.includes('-webgpu-') ||
        name.includes('-webgpu') ||
        name.includes('webgpu') ||
        name.includes('-generic-gpu')
    ) {
        return '#005A9C';
    }

    return 'currentColor';
}

export function getVariantLabel(variant: { name: string; deviceSupport: string[] }): string {
    const modelName = variant.name.toLowerCase();
    const device = variant.deviceSupport[0]?.toUpperCase() || '';

    if (modelName.includes('-cuda-gpu') || modelName.includes('-cuda-')) {
        if (
            modelName.includes('-trt-rtx-') ||
            modelName.includes('-tensorrt-') ||
            modelName.includes('-trtrtx-') ||
            modelName.includes('-trtrtx')
        ) {
            return `${device} (CUDA + TensorRT)`;
        }
        return `${device} (CUDA)`;
    } else if (modelName.includes('-generic-gpu') || modelName.includes('webgpu')) {
        return `${device} (WebGPU)`;
    }

    if (modelName.includes('-qnn-')) {
        return `${device} (QNN)`;
    } else if (modelName.includes('-vitis-')) {
        return `${device} (Vitis)`;
    } else if (modelName.includes('-openvino-')) {
        return `${device} (OpenVINO)`;
    } else if (
        modelName.includes('-trt-rtx-') ||
        modelName.includes('-tensorrt-') ||
        modelName.includes('-trtrtx-') ||
        modelName.includes('-trtrtx')
    ) {
        return `${device} (TensorRT)`;
    }

    if (modelName.includes('-generic-cpu')) {
        return `${device} (Generic)`;
    }

    return device;
}

export function getAcceleratorLogoFromAcceleration(acceleration: string): string | null {
    const accel = acceleration.toLowerCase();

    if (accel === 'cuda' || accel === 'trt-rtx' || accel === 'trtrtx') {
        return '/logos/nvidia-logo.svg';
    }
    if (accel === 'qnn') {
        return '/logos/qualcomm-logo.svg';
    }
    if (accel === 'vitis') {
        return '/logos/amd-logo.svg';
    }
    if (accel === 'openvino') {
        return '/logos/intel-logo.svg';
    }
    if (accel === 'webgpu') {
        return '/logos/webgpu-logo.svg';
    }

    return null;
}

export function getAcceleratorColorFromAcceleration(acceleration: string): string {
    const accel = acceleration.toLowerCase();
    const colors: Record<string, string> = {
        cuda: '#76B900',
        'trt-rtx': '#76B900',
        trtrtx: '#76B900',
        qnn: '#3253DC',
        vitis: 'var(--amd-color, #000000)',
        openvino: '#0071C5',
        webgpu: '#005A9C'
    };
    return colors[accel] || 'currentColor';
}
