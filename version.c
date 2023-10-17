
#define __VER__          GIT_HASH

//#define __VER_PREFIX__  "OEFW-"
#define __VER_PREFIX__   "1o11-"

const char Version_str[] = __VER_PREFIX__ __VER__;
#if defined(ENABLE_UART)
	const char UART_Version_str[] = "UV-K5 Firmware, Open Edition, " __VER_PREFIX__ __VER__ ", " __DATE__ " " __TIME__;
#endif
