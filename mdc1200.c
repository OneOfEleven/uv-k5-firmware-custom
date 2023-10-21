
#include <string.h>

#include "bsp/dp32g030/crc.h"
#include "mdc1200.h"
#include "misc.h"

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
uint16_t reverse_bits(const uint16_t bits_in, const unsigned int num_bits)
{
	uint16_t i;
	uint16_t bit;
	uint16_t bits_out;
	for (i = 1u << (num_bits - 1), bit = 1u, bits_out = 0u; i != 0; i >>= 1)
	{
		if (bits_in & i)
			 bits_out |= bit;
		bit <<= 1;
	}
	return bits_out;
}

#if 0
	uint16_t compute_crc(const uint8_t *data, const unsigned int data_len)
	{	// using the reverse computation avoids having to reverse the bit order during and after
		uint16_t crc = 0;
		for (i = 0; i < len; i++)
		{
			unsigned int k;
			crc ^= data[i];
			for (k = 8; k > 0; k--)
				crc = (crc & 1u) ? (crc >> 1) ^ 0x8408 : crc >> 1;
		}
		crc ^= 0xffff;
		return crc;
	}
#else
	uint16_t compute_crc(const uint8_t *data, const unsigned int data_len)
	{
	
		// this can be done using the CPU's own CRC calculator once we know we're ok
	
		unsigned int i;
	
		#if 0
			uint16_t crc;
	
			CRC_CR = (CRC_CR & ~CRC_CR_CRC_EN_MASK) | CRC_CR_CRC_EN_BITS_ENABLE;
		#else
			uint16_t crc = 0;
		#endif
	
		for (i = 0; i < data_len; i++)
		{
			#if 0
	
				// bit reverse each data byte before adding it to the CRC
				// the cortex CPU might have an instruction to bit reverse for us ?
				//
				CRC_DATAIN = reverse_bits(data[i], 8);
				//CRC_DATAIN = bitReverse8(data[i]);
	
			#else
				uint8_t mask;
		
				// bit reverse each data byte before adding it to the CRC
				// the cortex CPU might have an instruction to bit reverse for us ?
				//
				const uint8_t bits = reverse_bits(data[i], 8);
				//const uint8_t bits = bitReverse8(*data++);
	
				for (mask = 0x0080; mask != 0; mask >>= 1)
				{
					uint16_t msb = crc & 0x8000;
					if (bits & mask)
						msb ^= 0x8000;
					crc <<= 1;
					if (msb)
						crc ^= 0x1021;
				}
			#endif
		}
	
		#if 0
			crc    = (uint16_t)CRC_DATAOUT;
			CRC_CR = (CRC_CR & ~CRC_CR_CRC_EN_MASK) | CRC_CR_CRC_EN_BITS_DISABLE;
		#endif
		
		// bit reverse and invert the final CRC
		return reverse_bits(crc, 16) ^ 0xffff; 
	//	return bitReverse16(crc) ^ 0xffff;
	}
#endif

uint8_t * encode_data(uint8_t *data)
{
	unsigned int i;
	unsigned int k;
	unsigned int m;
	uint8_t      csr[7];
	uint8_t      lbits[(ARRAY_SIZE(csr) * 2) * 8];

	for (i = 0; i < ARRAY_SIZE(csr); i++)
		csr[i] = 0;

	for (i = 0; i < ARRAY_SIZE(csr); i++)
	{
		unsigned int bit;
		data[i + ARRAY_SIZE(csr)] = 0;
		for (bit = 0; bit < 8; bit++)
		{
			uint8_t b;
			for (k = 6; k > 0; k--)
				csr[k] = csr[k - 1];
			csr[0] = (data[i] >> bit) & 1u;
			b = csr[0] + csr[2] + csr[5] + csr[6];
			data[i + ARRAY_SIZE(csr)] |= (b & 1u) << bit;
		}
	}

	for (i = 0, k = 0, m = 0; i < (ARRAY_SIZE(csr) * 2); i++)
	{
		unsigned int bit;
		for (bit = 0; bit < 8; bit++)
		{
			lbits[k] = (data[i] >> bit) & 1u;
			k += 16;
			if (k >= ARRAY_SIZE(lbits))
				k = ++m;
		}
	}

	for (i = 0, k = 0; i < (ARRAY_SIZE(csr) * 2); i++)
	{
		int bit;
		data[i] = 0;
		for (bit = 7; bit >= 0; bit--)
			if (lbits[k++])
				data[i] |= 1u << bit;
	}

	return data + 14;
}

// MDC1200 sync bit reversals and packet header
//static const uint8_t header[] = {0x00, 0x00, 0x00, 0x05, 0x55, 0x55, 0x55, 0x07, 0x09, 0x2a, 0x44, 0x6f};
static const uint8_t header[] = {0x00, 0x00, 0x00, 0x0A, 0xAA, 0xAA, 0xAA, 0x07, 0x09, 0x2a, 0x44, 0x6f};

void delta_modulation(uint8_t *data, const unsigned int size)
{	// xor succesive bits in the entire packet, including the bit reversing pre-amble
	uint8_t b1;
	unsigned int i;
	for (i = 0, b1 = 1u; i < size; i++)
	{
		int bit_num;
		uint8_t in  = data[i];
		uint8_t out = 0;
		for (bit_num = 7; bit_num >= 0; bit_num--)
		{
			const uint8_t b2 = (in >> bit_num) & 1u;
//			const uint8_t b2 = (in >> (7 - bit_num)) & 1u;
			if (b1 != b2)
				out |= 1u << bit_num;        // previous bit and new bit are different
//				out |= 1u << (7 - bit_num);
			b1 = b2;
		}
		data[i] = out;
	}
}

unsigned int MDC1200_encode_single_packet(uint8_t *data, const uint8_t op, const uint8_t arg, const uint16_t unit_id)
{
	unsigned int size;
	uint8_t     *p = data;
	uint16_t     crc;

	memcpy(p, header, sizeof(header));
	p += sizeof(header);

	p[0] = op;
	p[1] = arg;
	p[2] = (unit_id >> 8) & 0x00ff;
	p[3] = (unit_id >> 0) & 0x00ff;
	crc = compute_crc(p, 4);
	p[4] = (crc >> 0) & 0x00ff;
	p[5] = (crc >> 8) & 0x00ff;
	p[6] = 0;

	p = encode_data(p);

	size = (unsigned int)(p - data);
	
	delta_modulation(data, size);
	
	return size;
//	return 26;
}

unsigned int MDC1200_encode_double_packet(uint8_t *data, const uint8_t op, const uint8_t arg, const uint16_t unit_id, const uint8_t b0, const uint8_t b1, const uint8_t b2, const uint8_t b3)
{
	unsigned int size;
	uint8_t *p = data;
	uint16_t     crc;

	memcpy(p, header, sizeof(header));
	p += sizeof(header);

	p[0] = op;
	p[1] = arg;
	p[2] = (unit_id >> 8) & 0x00ff;
	p[3] = (unit_id >> 0) & 0x00ff;
	crc = compute_crc(p, 4);
	p[4] = (crc >> 0) & 0x00ff;
	p[5] = (crc >> 8) & 0x00ff;
	p[6] = 0;

	p = encode_data(p);

	p[0] = b0;
	p[1] = b1;
	p[2] = b2;
	p[3] = b3;
	crc = compute_crc(p, 4);
	p[4] = (crc >> 0) & 0x00ff;
	p[5] = (crc >> 8) & 0x00ff;
	p[6] = 0;

	p = encode_data(p);

	size = (unsigned int)(p - data);
	
	delta_modulation(data, size);
	
//	return 40;
	return size;
}
/*
void test(void)
{
	uint8_t data[14 + 14 + 5 + 7];

	const int size = MDC1200_encode_single_packet(data, 0x12, 0x34, 0x5678);

//	const int size = MDC1200_encode_double_packet(data, 0x55, 0x34, 0x5678, 0x0a, 0x0b, 0x0c, 0x0d);
}
*/