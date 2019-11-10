#include "arch.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ARCH_X86)
  #include "prefix_x86_64.S"
  #include "context_x86.S"
#elif defined(ARCH_X64)
  #include "prefix_x86_64.S"
  #include "context_x64.S"
#elif defined(ARCH_ARM64)
  #include "prefix_arm64.S"
  #include "context_arm64.S"
#elif defined(ARCH_ARM)
  #include "prefix_arm.S"
  #include "context_arm.S"
#elif defined(ARCH_MIPS)
  #include "prefix_mips.S"
  #include "context_mips.S"
#endif

#ifdef __cplusplus
}
#endif
