
#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
	#include "driver/uart.h"
#endif
#include "freq_ignore.h"
#include "misc.h"

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

	if (frequency == 0 || frequency == 0xffffffff || ignore_frequencies_count <= 0)
		return -1;

	if (ignore_frequencies_count >= 8)
	{	// binary search .. becomes much faster than sequencial search when the list is bigger
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
			{
				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
					UART_printf("ignored bin %u %u\r\n", frequency, mid);
				#endif
				return mid;
			}
		}
	}
	else
	{	// sequencial search
		int i;
		for (i = 0; i < ignore_frequencies_count; i++)
		{
			register uint32_t freq = ignore_frequencies[i];
			if (frequency == freq)
			{	// found it
				#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
					UART_printf("ignored seq %u %u\r\n", frequency, i);
				#endif
				return i;
			}
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
		UART_printf("ignore add %u\r\n", frequency);
	#endif

	if (frequency == 0 || frequency == 0xffffffff)
		return;

	if (ignore_frequencies_count >= (int)ARRAY_SIZE(ignore_frequencies))
	{	// the list is full
		#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
			UART_SendText("ignore add full\r\n");
		#endif
		return;
	}

	for (i = 0; i < ignore_frequencies_count; i++)
	{
		register uint32_t freq = ignore_frequencies[i];

		if (frequency == freq)
		{	// already in the list
			#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
				UART_SendText("ignore add already\r\n");
			#endif
			return;
		}

		if (frequency < freq)
			break;
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
}

void FI_sub_freq_ignored(const uint32_t frequency)
{	// remove a frequency from the ignore list

	#if defined(ENABLE_UART) && defined(ENABLE_UART_DEBUG)
		UART_printf("ignore sub %u\r\n", frequency);
	#endif

	if (frequency == 0 || frequency == 0xffffffff)
		return;

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
