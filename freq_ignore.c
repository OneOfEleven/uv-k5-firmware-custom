
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "freq_ignore.h"
#include "misc.h"

//#define FI_CLOSE_ENOUGH_HZ   300

// a list of frequencies to ignore/skip when scanning
uint32_t ignore_frequencies[64];
int      ignore_frequencies_count = 0;

void FI_clear_freq_ignored(void)
{	// clear the ignore list
	ignore_frequencies_count = 0;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_SendText("ignore cleared\r\n");
	#endif
}

int FI_freq_ignored(const uint32_t frequency)
{	// return index of the ignored frequency

	#ifdef FI_CLOSE_ENOUGH_HZ
		if (frequency <= FI_CLOSE_ENOUGH_HZ || frequency >= (0xffffffff - FI_CLOSE_ENOUGH_HZ) || ignore_frequencies_count <= 0)
			return -1;   // invalid frequency
	#else
		if (frequency == 0 || frequency == 0xffffffff || ignore_frequencies_count <= 0)
			return -1;   // invalid frequency
	#endif

	if (ignore_frequencies_count >= 20)
	{	// binary search becomes faster than sequencial as the list grows beyound a certain size
		int low = 0;
		int high = ignore_frequencies_count;

		while (low < high)
		{
			register int mid  = (low + high) / 2;
			register uint32_t freq = ignore_frequencies[mid];

			#ifdef FI_CLOSE_ENOUGH_HZ
				if (abs((int32_t)frequency - (int32_t)freq) <= FI_CLOSE_ENOUGH_HZ)
			#else
				if (frequency == freq)
			#endif
			{	// found it
				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
					UART_printf("ignored bin %u %u\r\n", frequency, mid);
				#endif
				return mid;
			}

			if (freq > frequency)
				high = mid;
			else
				low  = mid + 1;
		}
	}
	else
	{	// sequencial search
		register int i;
		for (i = 0; i < ignore_frequencies_count; i++)
		{
			register uint32_t freq = ignore_frequencies[i];

			#ifdef FI_CLOSE_ENOUGH_HZ
				if (abs((int32_t)frequency - (int32_t)freq) <= FI_CLOSE_ENOUGH_HZ)
			#else
				if (frequency == freq)
			#endif
			{	// found it
				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
					UART_printf("ignored seq %u %u\r\n", frequency, i);
				#endif
				return i;
			}

			if (frequency < freq)
				return -1;        // exit loop early as the list is sorted by frequency
		}
	}

	return -1;    // not found
}

bool FI_add_freq_ignored(const uint32_t frequency)
{	// add a new frequency to the ignore list

	int i;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_printf("ignore add %u\r\n", frequency);
	#endif

	#ifdef FI_CLOSE_ENOUGH_HZ
		if (frequency <= FI_CLOSE_ENOUGH_HZ || frequency >= (0xffffffff - FI_CLOSE_ENOUGH_HZ))
			return false;   // invalid frequency
	#else
		if (frequency == 0 || frequency == 0xffffffff)
			return false;   // invalid frequency
	#endif

	if (ignore_frequencies_count >= (int)ARRAY_SIZE(ignore_frequencies))
	{	// the list is full
		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			UART_SendText("ignore add full\r\n");
		#endif
		return false;  // failed
	}

	for (i = 0; i < ignore_frequencies_count; i++)
	{
		register uint32_t freq = ignore_frequencies[i];

		#ifdef FI_CLOSE_ENOUGH_HZ
			if (abs((int32_t)frequency - (int32_t)freq) <= FI_CLOSE_ENOUGH_HZ)
		#else
			if (frequency == freq)
		#endif
		{	// already in the list
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("ignore add already\r\n");
			#endif
			return true;
		}

		#ifdef FI_CLOSE_ENOUGH_HZ
			if (frequency < (freq + FI_CLOSE_ENOUGH_HZ))
				break;        // exit loop early as the list is sorted by frequency
		#else
			if (frequency < freq)
				break;        // exit loop early as the list is sorted by frequency
		#endif
	}

	// found the location to store the new frequency - the list is kept sorted by frequency

	// make room for the new frequency
	if (i < ignore_frequencies_count)
		memmove(&ignore_frequencies[i + 1], &ignore_frequencies[i], sizeof(ignore_frequencies[0]) * (ignore_frequencies_count - i));

	// add the frequency to the list
	ignore_frequencies[i] = frequency;
	ignore_frequencies_count++;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		for (i = 0; i < ignore_frequencies_count; i++)
			UART_printf("%2u %10u\r\n", i, ignore_frequencies[i]);
	#endif

	return true;
}

void FI_sub_freq_ignored(const uint32_t frequency)
{	// remove a frequency from the ignore list

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_printf("ignore sub %u\r\n", frequency);
	#endif

	int index = FI_freq_ignored(frequency);
	if (index < 0)
		return;

	if (index < (ignore_frequencies_count - 1))
		memmove(&ignore_frequencies[index], &ignore_frequencies[index + 1], sizeof(ignore_frequencies[0]) * (ignore_frequencies_count - 1));
	ignore_frequencies_count--;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		for (index = 0; index < ignore_frequencies_count; index++)
			UART_printf("%2u %10u\r\n", index, ignore_frequencies[index]);
	#endif
}
