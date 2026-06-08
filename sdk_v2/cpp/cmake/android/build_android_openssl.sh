#!/usr/bin/env bash
set -e

# Ensure perl and make exist
command -v perl >/dev/null || { echo "Perl is required."; exit 1; }
command -v make >/dev/null || { echo "make is required."; exit 1; }

# Path to your Android NDK
# Download from https://developer.android.com/ndk/downloads
export ANDROID_NDK_HOME=$HOME/Android/android-ndk-r27d
export PATH=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH

# OpenSSL version or branch (optional)
# Use the latest 1.1.x as it's a ~50% smaller than 3.x 
# Downside is we need to patch the build setup to not create versioned .so files
# as Android doesn't support that.
OPENSSL_VERSION="OpenSSL_1_1_1w"

# Architectures to build
ABIS=(
  "android-arm64"
  "android-x86_64"
)

# Clone OpenSSL if not already present
if [ ! -d "openssl" ]; then
  git clone https://github.com/openssl/openssl.git
  cd openssl
  git checkout $OPENSSL_VERSION
  git apply ../openssl_build_noversion_and_16kb_pagesize.patch
  cd ..
fi

# Loop over ABIs
for TARGET in "${ABIS[@]}"; do
  BUILD_DIR="openssl_build_${TARGET}"
  INSTALL_DIR="openssl_install_${TARGET}"

  echo "=============================="
  echo " Building OpenSSL for $TARGET "
  echo "=============================="

  rm -rf "$BUILD_DIR" "$INSTALL_DIR"
  mkdir -p "$BUILD_DIR"
  cd "$BUILD_DIR"

  # Run OpenSSL Configure.
  # TODO: There are more things we could disable but using the full list below results in 
  # System.Security.Crytography.Native silently failing when loading/validation libssl.so.
  # It has a long list of required things (see https://github.com/dotnet/runtime/blob/main/src/native/libs/System.Security.Cryptography.Native/opensslshim.h#L300).
  perl ../openssl/Configure $TARGET shared \
    no-ui-console no-tests
    #no-ssl3 no-tls1 no-tls1_1 \
    #no-md2 no-md4 no-rc2 no-rc4 no-bf no-idea no-cast no-mdc2 \
    #enable-ec_nistp_64_gcc_128

  # Build and install
  # make SHLIB_VERSION_NUMBER= -j$(nproc)
  # make SHLIB_VERSION_NUMBER= install DESTDIR="$(pwd)/../$INSTALL_DIR"
  make -j$(nproc)
  make install DESTDIR="$(pwd)/../$INSTALL_DIR"

  cd ..

  echo "✅ Finished building for $TARGET"
  echo "   -> $(pwd)/$INSTALL_DIR/usr/local/lib/libssl.so"
done

echo "🎉 All builds completed!"
