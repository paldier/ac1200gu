#
# Automatically generated make config: don't edit
#
# TARGET_alpha is not set
# TARGET_arm is not set
# TARGET_bfin is not set
# TARGET_cris is not set
# TARGET_e1 is not set
# TARGET_frv is not set
# TARGET_h8300 is not set
# TARGET_i386 is not set
# TARGET_i960 is not set
# TARGET_m68k is not set
# TARGET_microblaze is not set
TARGET_mips=y
# TARGET_nios is not set
# TARGET_nios2 is not set
# TARGET_powerpc is not set
# TARGET_sh is not set
# TARGET_sh64 is not set
# TARGET_sparc is not set
# TARGET_v850 is not set
# TARGET_x86_64 is not set

#
# Target Architecture Features and Options
#
HAVE_ELF=y
ARCH_SUPPORTS_LITTLE_ENDIAN=y
TARGET_ARCH="mips"
ARCH_CFLAGS="-mno-split-addresses"
ARCH_SUPPORTS_BIG_ENDIAN=y
CONFIG_MIPS_ISA_1=y
# CONFIG_MIPS_ISA_2 is not set
# CONFIG_MIPS_ISA_3 is not set
# CONFIG_MIPS_ISA_4 is not set
# CONFIG_MIPS_ISA_MIPS32 is not set
# CONFIG_MIPS_ISA_MIPS64 is not set
ARCH_LITTLE_ENDIAN=y
# ARCH_BIG_ENDIAN is not set
# ARCH_HAS_NO_MMU is not set
ARCH_HAS_MMU=y
UCLIBC_HAS_FLOATS=y
HAS_FPU=y
DO_C99_MATH=y
KERNEL_SOURCE="$(ROOTDIR)/$(LINUXDIR)"
C_SYMBOL_PREFIX=""
HAVE_DOT_CONFIG=y

#
# General Library Settings
#
# HAVE_NO_PIC is not set
DOPIC=y
# HAVE_NO_SHARED is not set
HAVE_SHARED=y
# ARCH_HAS_NO_LDSO is not set
BUILD_UCLIBC_LDSO=y
# OPTIMIZED_SHARED is not set
FORCE_SHAREABLE_TEXT_SEGMENTS=y
LDSO_LDD_SUPPORT=y
LDSO_CACHE_SUPPORT=y
# LDSO_PRELOAD_FILE_SUPPORT is not set
LDSO_BASE_FILENAME="ld.so"
LDSO_RUNPATH=y
DL_FINI_CRT_COMPAT=y
UCLIBC_CTOR_DTOR=y
# HAS_NO_THREADS is not set
UCLIBC_HAS_THREADS=y
# PTHREADS_DEBUG_SUPPORT is not set
UCLIBC_HAS_LFS=y
UCLIBC_STATIC_LDCONFIG=y
# MALLOC is not set
# MALLOC_SIMPLE is not set
MALLOC_STANDARD=y
# MALLOC_GLIBC_COMPAT is not set
UCLIBC_DYNAMIC_ATEXIT=y
HAS_SHADOW=y
UNIX98PTY_ONLY=y
ASSUME_DEVPTS=y
UCLIBC_HAS_TM_EXTENSIONS=y
UCLIBC_HAS_TZ_CACHING=y
UCLIBC_HAS_TZ_FILE=y
UCLIBC_HAS_TZ_FILE_READ_MANY=y
UCLIBC_TZ_FILE_PATH="/etc/TZ"

#
# Networking Support
#
UCLIBC_HAS_IPV6=y
UCLIBC_HAS_RPC=y
UCLIBC_HAS_FULL_RPC=y

#
# String and Stdio Support
#
UCLIBC_HAS_STRING_GENERIC_OPT=y
UCLIBC_HAS_STRING_ARCH_OPT=y
UCLIBC_HAS_CTYPE_TABLES=y
UCLIBC_HAS_CTYPE_SIGNED=y
UCLIBC_HAS_CTYPE_UNSAFE=y
# UCLIBC_HAS_CTYPE_CHECKED is not set
# UCLIBC_HAS_CTYPE_ENFORCED is not set
UCLIBC_HAS_WCHAR=y
# UCLIBC_HAS_LOCALE is not set
# UCLIBC_HAS_HEXADECIMAL_FLOATS is not set
# UCLIBC_HAS_GLIBC_CUSTOM_PRINTF is not set
UCLIBC_PRINTF_SCANF_POSITIONAL_ARGS=9
# UCLIBC_HAS_SCANF_GLIBC_A_FLAG is not set
# UCLIBC_HAS_STDIO_BUFSIZ_NONE is not set
# UCLIBC_HAS_STDIO_BUFSIZ_256 is not set
# UCLIBC_HAS_STDIO_BUFSIZ_512 is not set
# UCLIBC_HAS_STDIO_BUFSIZ_1024 is not set
# UCLIBC_HAS_STDIO_BUFSIZ_2048 is not set
UCLIBC_HAS_STDIO_BUFSIZ_4096=y
# UCLIBC_HAS_STDIO_BUFSIZ_8192 is not set
UCLIBC_HAS_STDIO_BUILTIN_BUFFER_NONE=y
# UCLIBC_HAS_STDIO_BUILTIN_BUFFER_4 is not set
# UCLIBC_HAS_STDIO_BUILTIN_BUFFER_8 is not set
# UCLIBC_HAS_STDIO_SHUTDOWN_ON_ABORT is not set
UCLIBC_HAS_STDIO_GETC_MACRO=y
UCLIBC_HAS_STDIO_PUTC_MACRO=y
UCLIBC_HAS_STDIO_AUTO_RW_TRANSITION=y
# UCLIBC_HAS_FOPEN_LARGEFILE_MODE is not set
# UCLIBC_HAS_FOPEN_EXCLUSIVE_MODE is not set
# UCLIBC_HAS_GLIBC_CUSTOM_STREAMS is not set
# UCLIBC_HAS_PRINTF_M_SPEC is not set
UCLIBC_HAS_ERRNO_MESSAGES=y
# UCLIBC_HAS_SYS_ERRLIST is not set
UCLIBC_HAS_SIGNUM_MESSAGES=y
# UCLIBC_HAS_SYS_SIGLIST is not set
UCLIBC_HAS_GNU_GETOPT=y

#
# Big and Tall
#
UCLIBC_HAS_REGEX=y
# UCLIBC_HAS_WORDEXP is not set
# UCLIBC_HAS_FTW is not set
UCLIBC_HAS_GLOB=y

#
# Library Installation Options
#
SHARED_LIB_LOADER_PREFIX="/lib"
RUNTIME_PREFIX="$(ROOTDIR)/romfs"
DEVEL_PREFIX="$(ROOTDIR)/romfs"

#
# uClibc security related options
#
UCLIBC_SECURITY=y
UCLIBC_BUILD_PIE=y
# UCLIBC_HAS_SSP is not set
UCLIBC_BUILD_RELRO=y
UCLIBC_BUILD_NOW=y
UCLIBC_BUILD_NOEXECSTACK=y

#
# uClibc development/debugging options
#
CROSS_COMPILER_PREFIX=""
# DODEBUG is not set
# DODEBUG_PT is not set
# DOASSERTS is not set
# SUPPORT_LD_DEBUG is not set
# SUPPORT_LD_DEBUG_EARLY is not set
WARNINGS="-Wall"
# UCLIBC_MJN3_ONLY is not set
