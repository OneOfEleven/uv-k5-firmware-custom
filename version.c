
#ifdef GIT_HASH
	#define VER     GIT_HASH
#else
	#define VER     "230920"
#endif

const char Version[]      = "CRFW-"VER;
const char UART_Version[] = "UV-K5 Firmware, Open Edition, CRFW-"VER"\r\n";
