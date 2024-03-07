/* Default visibility */
#if defined(__GNUC__) || defined(__clang__)
  #define DEFAULT_VISIBILITY __attribute__((visibility("default")))
#elif defined(_MSC_VER)
  #define DEFAULT_VISIBILITY __declspec(dllexport)
#endif

/* Start with debug message logging enabled */
#undef ENABLE_DEBUG_LOGGING

/* Message logging */
#undef ENABLE_LOGGING

/* Define to 1 if you have the <asm/types.h> header file. */
#define HAVE_ASM_TYPES_H 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the `udev' library (-ludev). */
/* #undef HAVE_LIBUDEV */

/* Define to 1 if you have the <linux/filter.h> header file. */
#define HAVE_LINUX_FILTER_H 1

/* Define to 1 if you have the <linux/netlink.h> header file. */
#define HAVE_LINUX_NETLINK_H 1

/* Define to 1 if you have eventfd support. */
#define HAVE_EVENTFD 1

/* Define to 1 if you have timerfd support. */
#define HAVE_TIMERFD 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if the system has the type `struct timespec'. */
#define HAVE_STRUCT_TIMESPEC 1

/* syslog() function available */
#define HAVE_SYSLOG_FUNC 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the 'clock_gettime' function. */
#define HAVE_CLOCK_GETTIME 1

/* Darwin backend */
/* #undef OS_DARWIN */

/* Linux backend */
#define OS_LINUX 1

/* NetBSD backend */
/* #undef OS_NETBSD */

/* OpenBSD backend */
/* #undef OS_OPENBSD */

/* Windows backend */
/* #undef OS_WINDOWS */

/* Use POSIX Platform */
#define PLATFORM_POSIX

/* Use Windows Platform */
/* #undef PLATFORM_WINDOWS */

/* Enable output to system log */
#define USE_SYSTEM_LOGGING_FACILITY 1

/* Use udev for device enumeration/hotplug */
/* #undef USE_UDEV */

/* Use GNU extensions */
#define _GNU_SOURCE

/* Oldest Windows version supported */
#define WINVER 0x0501
