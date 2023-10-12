
#define ONE_OF_ELEVEN_VER

#ifdef GIT_HASH
	#define VER     GIT_HASH
#else
	#define VER     "231012"
#endif

#ifndef ONE_OF_ELEVEN_VER
	const char Version_str[]      = "OEFW-"VER;
	const char UART_Version_str[] = "UV-K5 Firmware, Open Edition, OEFW-"VER"\r\n";
#else
	const char Version_str[]      = "1o11-"VER;
	const char UART_Version_str[] = "UV-K5 Firmware, Open Edition, 1o11-"VER"\r\n";
#endif
