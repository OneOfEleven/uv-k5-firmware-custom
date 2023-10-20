
#include <string.h>

#include "mdc1200.h"
/*
uint8_t bitReverse8(uint8_t n)
{
	n = ((n >> 1) & 0x55u) | ((n << 1) & 0xAAu);
	n = ((n >> 2) & 0x33u) | ((n << 2) & 0xCCu);
	n = ((n >> 4) & 0x0Fu) | ((n << 4) & 0xF0u);
	return n;
}

uint16_t bitReverse16(uint16_t n)
{	// untested
	n = ((n >> 1) & 0x5555u) | ((n << 1) & 0xAAAAu);
	n = ((n >> 2) & 0x3333u) | ((n << 2) & 0xCCCCu);
	n = ((n >> 4) & 0x0F0Fu) | ((n << 4) & 0xF0F0u);
	n = ((n >> 8) & 0x00FFu) | ((n << 8) & 0xFF00u);
   return n;
}

uint32_t bitReverse32(uint32_t n)
{
	n = ((n >>  1) & 0x55555555u) | ((n <<  1) & 0xAAAAAAAAu);
	n = ((n >>  2) & 0x33333333u) | ((n <<  2) & 0xCCCCCCCCu);
	n = ((n >>  4) & 0x0F0F0F0Fu) | ((n <<  4) & 0xF0F0F0F0u);
	n = ((n >>  8) & 0x00FF00FFu) | ((n <<  8) & 0xFF00FF00u);
	n = ((n >> 16) & 0x0000FFFFu) | ((n << 16) & 0xFFFF0000u);
	return n;
}
*/
uint16_t reverse_bits(const uint16_t bits_in, const unsigned int bit_num)
{
	uint16_t i;
	uint16_t bit;
	uint16_t bits_out;
	for (i = 1u << (bit_num - 1), bit = 1u, bits_out = 0u; i > 0u; i >>= 1)
	{
		if (bits_in & i)
			 bits_out |= bit;
		bit <<= 1;
	}
	return bits_out;
}

uint16_t compute_crc(const uint8_t *data, const unsigned int data_len)
{
	unsigned int i;
	uint16_t crc = 0x0000;

	for (i = 0; i < data_len; i++)
	{
		uint16_t mask;
		
		const uint16_t b = reverse_bits(*data++, 8); // bit reverse each data byte
		
		for (mask = 0x0080; mask > 0; mask >>= 1)
		{
			uint16_t bit = crc & 0x8000;
			crc <<= 1;
			if (b & mask)
				bit ^= 0x8000;
			if (bit)
				crc ^= 0x1021;
		}
	}

	return reverse_bits(crc, 16) ^ 0xffff; // bit reverse and invert the CRC
}

uint8_t * encode_data(uint8_t *data)
{
	unsigned int i;
	unsigned int k;
	unsigned int m;
	int      csr[7];
	int      lbits[112];

	const uint16_t ccrc = compute_crc(data, 4);
	data[4] = (ccrc >> 0) & 0x00ff;
	data[5] = (ccrc >> 8) & 0x00ff;

	data[6] = 0;

	for (i = 0; i < 7; i++)
		csr[i] = 0;

	for (i = 0; i < 7; i++)
	{
		unsigned int j;
		data[i + 7] = 0;
		for (j = 0; j <= 7; j++)
		{
			unsigned int b;
			for (k = 6; k > 0; k--)
				csr[k] = csr[k - 1];
			csr[0] = (data[i] >> j) & 1u;
			b = csr[0] + csr[2] + csr[5] + csr[6];
			data[i + 7] |= (b & 1u) << j;
		}
	}

	k = 0;
	m = 0;
	for (i = 0; i < 14; i++)
	{
		unsigned int j;
		for (j = 0; j <= 7; j++)
		{
			lbits[k] = 1u & (data[i] >> j);
			k += 16;
			if (k > 111)
				k = ++m;
		}
	}

	k = 0;
	for (i = 0; i < 14; i++)
	{
		int j;
		data[i] = 0;
		for (j = 7; j >= 0; j--)
			if (lbits[k++])
				data[i] |= 1u << j;
	}

	return &data[14];
}

const uint8_t header[] = {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x07, 0x09, 0x2a, 0x44, 0x6f};

unsigned int MDC1200_encode_single_packet(uint8_t *data, const uint8_t op, const uint8_t arg, const uint16_t unit_id)
{
	uint8_t *p = data;

	#if 0
		memcpy(p, header, sizeof(header));
		p += sizeof(header);
	#else
		memcpy(p + 7, header, sizeof(header));
		p += sizeof(header) - 7;
	#endif

	p[0] = op;
	p[1] = arg;
	p[2] = (unit_id >> 8) & 0x00ff;
	p[3] = (unit_id >> 0) & 0x00ff;

	p = encode_data(p);

	return (unsigned int)(p - data);
//	return 26;
}

unsigned int MDC1200_encode_double_packet(uint8_t *data, const uint8_t op, const uint8_t arg, const uint16_t unit_id, const uint8_t b0, const uint8_t b1, const uint8_t b2, const uint8_t b3)
{
	uint8_t *p = data;

	#if 0
		memcpy(p, header, sizeof(header));
		p += sizeof(header);
	#else
		memcpy(p + 7, header, sizeof(header));
		p += sizeof(header) - 7;
	#endif

	p[0] = op;
	p[1] = arg;
	p[2] = (unit_id >> 8) & 0x00ff;
	p[3] = (unit_id >> 0) & 0x00ff;

	p = encode_data(p);

	p[0] = b0;
	p[1] = b1;
	p[2] = b2;
	p[3] = b3;

	p = encode_data(p);

//	return 40;
	return (unsigned int)(p - data);
}
/*
void test(void)
{
	uint8_t data[14 + 14 + 5 + 7];

	const int size = MDC1200_encode_single_packet(data, 0x12, 0x34, 0x5678);

//	const int size = MDC1200_encode_double_packet(data, 0x55, 0x34, 0x5678, 0x0a, 0x0b, 0x0c, 0x0d);
}
*/