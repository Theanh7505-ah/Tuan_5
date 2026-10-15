#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p external
clone_if_missing() {
  local dest="$1" url="$2" branch="${3:-}"
  if [[ -d "$dest/.git" ]]; then
    echo "[OK] $dest exists"
    return
  fi
  if [[ -e "$dest" ]]; then
    echo "[ERROR] $dest exists but is not a git repository" >&2; exit 1
  fi
  if [[ -n "$branch" ]]; then
    git clone --depth 1 --branch "$branch" "$url" "$dest"
  else
    git clone --depth 1 "$url" "$dest"
  fi
}
clone_if_missing external/STM32F1xx_HAL_Driver https://github.com/STMicroelectronics/stm32f1xx_hal_driver.git
clone_if_missing external/cmsis_device_f1 https://github.com/STMicroelectronics/cmsis_device_f1.git
clone_if_missing external/CMSIS_5 https://github.com/ARM-software/CMSIS_5.git
clone_if_missing external/FreeRTOS-Kernel https://github.com/FreeRTOS/FreeRTOS-Kernel.git V10.5.1
printf '
Dependencies:
'
for d in external/*; do printf '%s: ' "$d"; git -C "$d" rev-parse --short HEAD; done
