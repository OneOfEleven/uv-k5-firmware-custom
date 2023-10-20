
#include <string.h>

#include "mdc1200.h"

uint16_t flip_crc(const uint16_t crc, const unsigned int bit_num)
{
	uint16_t i;
	uint16_t bit;
	uint16_t crc_out;
	for (i = 1u << (bit_num - 1), bit = 1u, crc_out = 0u; i > 0u; i >>= 1)
	{
		if (crc & i)
			 crc_out |= bit;
		bit <<= 1;
	}
	return crc_out;
}

uint16_t compute_crc(const uint8_t *data, const unsigned int data_len)
{
	unsigned int i;
	uint16_t crc = 0x0000;

	for (i = 0; i < data_len; i++)
	{
		unsigned int mask;
		const uint16_t c = flip_crc(*data++, 8);
		for (mask = 0x80; mask > 0; mask >>= 1)
		{
			uint16_t bit = crc & 0x8000;
			crc <<= 1;
			if (c & mask)
				bit ^= 0x8000;
			if (bit)
				crc ^= 0x1021;
		}
	}

	return ~flip_crc(crc, 16);
}

uint8_t * encode_data(uint8_t *data)
{
	unsigned int i;
	unsigned int k;
	unsigned int m;
	int      csr[7];
	int      lbits[112];

	uint16_t ccrc = compute_crc(data, 4);

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

int MDC1200_encode_single_packet(uint8_t *data, const uint8_t op, const uint8_t arg, const uint16_t unit_id)
{
	memcpy(data, header, sizeof(header));
	data += sizeof(header);

	data[0] = op;
	data[1] = arg;
	data[2] = (unit_id >> 8) & 0x00ff;
	data[3] = (unit_id >> 0) & 0x00ff;

	encode_data(data);

	return 26;
}

int MDC1200_encode_double_packet(uint8_t *data, const uint8_t op, const uint8_t arg, const uint16_t unit_id, const uint8_t b0, const uint8_t b1, const uint8_t b2, const uint8_t b3)
{
	memcpy(data, header, sizeof(header));
	data += sizeof(header);

	data[0] = op;
	data[1] = arg;
	data[2] = (unit_id >> 8) & 0x00ff;
	data[3] = (unit_id >> 0) & 0x00ff;

	data = encode_data(data);

	data[0] = b0;
	data[1] = b1;
	data[2] = b2;
	data[3] = b3;

	encode_data(data);

	return 40;
}
/*
void test(void)
{
	uint8_t data[14 + 14 + 5 + 7];

	const int size = MDC1200_encode_single_packet(data, 0x12, 0x34, 0x5678);

//	const int size = MDC1200_encode_double_packet(data, 0x55, 0x34, 0x5678, 0x0a, 0x0b, 0x0c, 0x0d);
}
*/