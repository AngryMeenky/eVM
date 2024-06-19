#ifndef EVM_EVM_CONFIG_H
#  define EVM_EVM_CONFIG_H


// config values
// The maximum number of different builtin functions that can be called by the byte code
// valid values: [4,256]
#define EVM_MAX_BUILTINS (32)

// Does the target platfrom allow unaligned reads?
// valid values: [0,1]
#define EVM_UNALIGNED_READS (0)

// Support floating point math?
// valid values: [0,1]
#define EVM_FLOAT_SUPPORT (1)

// what level of logging to support?
// valid values: [0,6]
#define EVM_LOG_LEVEL (4)


// logging support
#define EVM_TRACE(STR) EVM_TRACEF("%s", STR)
#define EVM_DEBUG(STR) EVM_DEBUGF("%s", STR)
#define EVM_INFO(STR)  EVM_INFOF("%s", STR)
#define EVM_WARN(STR)  EVM_WARNF("%s", STR)
#define EVM_ERROR(STR) EVM_ERRORF("%s", STR)
#define EVM_FATAL(STR) EVM_FATALF("%s", STR)

#if defined(EVM_LOG_LEVEL) && EVM_LOG_LEVEL >= 6
#  define EVM_TRACEF(FMT, ...) fprintf(stderr, "TRACE: " FMT "\n", __VA_ARGS__)
#else
#  define EVM_TRACEF(FMT, ...) while(0)
#endif

#if defined(EVM_LOG_LEVEL) && EVM_LOG_LEVEL >= 5
#  define EVM_DEBUGF(FMT, ...) fprintf(stderr, "DEBUG: " FMT "\n", __VA_ARGS__)
#else
#  define EVM_DEBUGF(FMT, ...) while(0)
#endif

#if defined(EVM_LOG_LEVEL) && EVM_LOG_LEVEL >= 4
#  define EVM_INFOF(FMT, ...) fprintf(stderr, "INFO: " FMT "\n", __VA_ARGS__)
#else
#  define EVM_INFOF(FMT, ...) while(0)
#endif

#if defined(EVM_LOG_LEVEL) && EVM_LOG_LEVEL >= 3
#  define EVM_WARNF(FMT, ...) fprintf(stderr, "WARN: " FMT "\n", __VA_ARGS__)
#else
#  define EVM_WARNF(FMT, ...) while(0)
#endif

#if defined(EVM_LOG_LEVEL) && EVM_LOG_LEVEL >= 2
#  define EVM_ERRORF(FMT, ...) fprintf(stderr, "ERROR: " FMT "\n", __VA_ARGS__)
#else
#  define EVM_ERRORF(FMT, ...) while(0)
#endif

#if defined(EVM_LOG_LEVEL) && EVM_LOG_LEVEL >= 1
#  define EVM_FATALF(FMT, ...) do { \
    fprintf(stderr, "FATAL: " FMT "\n", __VA_ARGS__); \
    fflush(stderr); \
    exit(1); \
  } while(0)
#else
#  define EVM_FATALF(FMT, ...) exit(1)
#endif


// config validation
#if !defined(EVM_MAX_BUILTINS)
#  error "EVM_MAX_BUILTINS is undefined"
#elif EVM_MAX_BUILTINS < 0 || EVM_MAX_BUILTINS > 256
#  error "EVM_MAX_BUILTINS is out of range"
#endif

#if !defined(EVM_UNALIGNED_READS)
#  error "EVM_UNALIGNED_READS is undefined"
#elif EVM_UNALIGNED_READS < 0 || EVM_UNALIGNED_READS > 1
#  error "EVM_UNALIGNED_READS is out of range"
#endif

#if !defined(EVM_FLOAT_SUPPORT)
#  error "EVM_FLOAT_SUPPORT is undefined"
#elif EVM_FLOAT_SUPPORT < 0 || EVM_FLOAT_SUPPORT > 1
#  error "EVM_FLOAT_SUPPORT is out of range"
#endif

#if !defined(EVM_LOG_LEVEL)
#  error "EVM_LOG_LEVEL is undefined"
#elif EVM_LOG_LEVEL < 0 || EVM_LOG_LEVEL > 6
#  error "EVM_LOG_LEVEL is out of range"
#endif

#endif

