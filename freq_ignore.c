
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "freq_ignore.h"
#include "misc.h"

// a list of frequencies to ignore/skip when scanning
uint32_t ignore_frequencies[256];
int      ignore_frequencies_count = 0;

void FI_clear_freq_ignored(void)
{	// clear the ignore list
	ignore_frequencies_count = 0;
}

int FI_freq_ignored(const uint32_t frequency)
{	// return index of the ignored frequency

	if (frequency == 0 || frequency == 0xffffffff || ignore_frequencies_count <= 0)
		return -1;

	if (ignore_frequencies_count > 4)
	{	// binary search .. becomes much faster than sequencial as the list grows
		int low = 0;
		int high = ignore_frequencies_count;
		while (low < high)
		{
			register int mid  = (low + high) / 2;
			register uint32_t freq = ignore_frequencies[mid];
			if (freq > frequency)
				high = mid;
			else
			if (freq < frequency)
				low = mid + 1;
			else
				return mid;
		}
	}
	else
	{	// sequencial search
		int i;
		for (i = 0; i < ignore_frequencies_count; i++)
		{
			register uint32_t freq = ignore_frequencies[i];
			if (frequency == freq)
				return i;         // found it
			if (frequency < freq)
				return -1;        // can exit loop early as the list is sorted by frequency
		}
	}

	return -1;    // not found
}

void FI_add_freq_ignored(const uint32_t frequency)
{	// add a new frequency to the ignore list

	int i;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_printf("ignore %u\r\n", frequency);
	#endif

	if (ignore_frequencies_count >= (int)ARRAY_SIZE(ignore_frequencies))
	{	// the list is full
		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			UART_SendText("ignore full\r\n");
		#endif
		return;
	}

	for (i = 0; i < ignore_frequencies_count; i++)
	{
		register uint32_t freq = ignore_frequencies[i];

		if (frequency == freq)
		{	// already in the list
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("ignore already\r\n");
			#endif
			return;
		}

		if (frequency < freq)
			break;
	}

	// found the location to store the new frequency - the list is kept sorted by frequency

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_SendText("ignore adding ..\r\n");
	#endif

	// make room for the new frequency
	if (i < ignore_frequencies_count)
		memmove(&ignore_frequencies[i + 1], &ignore_frequencies[i], sizeof(ignore_frequencies[0]) * (ignore_frequencies_count - i - 1));

	// add the frequency to the list
	ignore_frequencies[i] = frequency;
	ignore_frequencies_count++;

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		for (i = 0; i < ignore_frequencies_count; i++)
			UART_printf("%2u %10u\r\n", i, ignore_frequencies[i]);
	#endif
}

void FI_sub_freq_ignored(const uint32_t frequency)
{	// remove a frequency from the ignore list

	int index = FI_freq_ignored(frequency);
	if (index >= 0)
	{
		if (index < (ignore_frequencies_count - 1))
			memmove(&ignore_frequencies[index], &ignore_frequencies[index + 1], sizeof(ignore_frequencies[0]) * (ignore_frequencies_count - 1));
		ignore_frequencies_count--;

		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			UART_SendText("ignore freq ..\r\n");
			for (index = 0; index < ignore_frequencies_count; index++)
				UART_printf("%2u %10u\r\n", index, ignore_frequencies[index]);
		#endif
	}
}
