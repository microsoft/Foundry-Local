# Test-only build image; no artifacts from this image are shipped.
FROM nvidia/cuda:13.0.2-cudnn-devel-ubuntu24.04@sha256:e071e85c52ad91fc9ea24158ff5330876b2d1a5c4ac83ccc6066976835873c01

# ORT 1.28's GPU NuGet uses CUDA 13; the selected GenAI nightly uses CUDA 12.
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        autoconf automake ca-certificates curl g++ git libtool make ninja-build \
        perl pkg-config python3 python3-venv tar unzip zip \
        cuda-cudart-12-8 libcublas-12-8 libcufft-12-8 libcurand-12-8 \
    && rm -rf /var/lib/apt/lists/*

ENV LD_LIBRARY_PATH=/usr/local/cuda-12.8/lib64:/usr/local/cuda-13.0/lib64:/usr/local/nvidia/lib:/usr/local/nvidia/lib64
