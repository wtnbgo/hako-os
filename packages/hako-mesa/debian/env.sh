# Environment for programs that should use this Mesa (sourced with set -a by
# /usr/lib/hako/session-run through a board's session.d file).
LD_LIBRARY_PATH=/usr/lib/hako/mesa/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/lib/hako/mesa/share/glvnd/egl_vendor.d/50_mesa.json
VK_DRIVER_FILES=/usr/lib/hako/mesa/share/vulkan/icd.d/freedreno_icd.json
