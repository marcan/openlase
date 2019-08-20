set(ASM_DIALECT "_YASM")
set(CMAKE_ASM${ASM_DIALECT}_SOURCE_FILE_EXTENSIONS asm)

if(APPLE)
  if(BITS EQUAL 64)
    set(CMAKE_ASM_YASM_COMPILER_ARG1 "-f macho64 -DARCH_X86_64=1 -Dprivate_prefix=ol -DPIC=1")
  else()
    set(CMAKE_ASM_YASM_COMPILER_ARG1 "-f macho32 -DARCH_X86_64=0 -Dprivate_prefix=ol")
  endif()
elseif(UNIX)
  if(BITS EQUAL 64)
    set(CMAKE_ASM_YASM_COMPILER_ARG1 "-f elf64 -DARCH_X86_64=1 -Dprivate_prefix=ol -DPIC=1")
  else()
    set(CMAKE_ASM_YASM_COMPILER_ARG1 "-f elf32 -DARCH_X86_64=0 -Dprivate_prefix=ol")
  endif()
else()
  if(BITS EQUAL 64)
    set(CMAKE_ASM_YASM_COMPILER_ARG1 "-f win64 -DARCH_X86_64=1 -Dprivate_prefix=ol -DPIC=1")
  else()
    set(CMAKE_ASM_YASM_COMPILER_ARG1 "-f win32 -DARCH_X86_64=0 -Dprivate_prefix=ol")
  endif()
endif()

# This section exists to override the one in CMakeASMInformation.cmake
# (the default Information file). This removes the <FLAGS>
# thing so that your C compiler flags that have been set via
# set_target_properties don't get passed to yasm and confuse it.
if(NOT CMAKE_ASM${ASM_DIALECT}_COMPILE_OBJECT)
  set(CMAKE_ASM${ASM_DIALECT}_COMPILE_OBJECT "<CMAKE_ASM${ASM_DIALECT}_COMPILER> -o <OBJECT> <SOURCE>")
endif()

include(CMakeASMInformation)
set(ASM_DIALECT)
