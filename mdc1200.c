
#include <string.h>

#include "bsp/dp32g030/crc.h"
#include "mdc1200.h"
#include "misc.h"

// MDC1200 sync bit reversals and packet magic
//
// 24-bit pre-amble
// 40-bit sync
//
//static const uint8_t header[] = {0x00, 0x00, 0x05, 0x55, 0x55, 0x55, 0x55, 0x07, 0x09, 0x2a, 0x44, 0x6f};
//static const uint8_t header[] = {0x00, 0x00, 0x0A, 0xAA, 0xAA, 0xAA, 0xAA, 0x07, 0x09, 0x2a, 0x44, 0x6f};
//
//static const uint8_t header[] = {0x00, 0x00, 0x0A, 0xAA, 0xAA, 0xAA, 0xA0, 0xb6, 0x8e, 0x03, 0xbb, 0x14};
static   const uint8_t header[] = {0x00, 0x00, 0x05, 0x55, 0x55, 0x55, 0x50, 0x29, 0x71, 0xfc, 0x44, 0xeb};

uint8_t bit_reverse_8(uint8_t n)
{
	n = ((n >> 1) & 0x55u) | ((n << 1) & 0xAAu);
	n = ((n >> 2) & 0x33u) | ((n << 2) & 0xCCu);
	n = ((n >> 4) & 0x0Fu) | ((n << 4) & 0xF0u);
	return n;
}

uint16_t bit_reverse_16(uint16_t n)
{	// untested
	n = ((n >> 1) & 0x5555u) | ((n << 1) & 0xAAAAu);
	n = ((n >> 2) & 0x3333u) | ((n << 2) & 0xCCCCu);
	n = ((n >> 4) & 0x0F0Fu) | ((n << 4) & 0xF0F0u);
	n = ((n >> 8) & 0x00FFu) | ((n << 8) & 0xFF00u);
   return n;
}

uint32_t bit_reverse_32(uint32_t n)
{
	n = ((n >>  1) & 0x55555555u) | ((n <<  1) & 0xAAAAAAAAu);
	n = ((n >>  2) & 0x33333333u) | ((n <<  2) & 0xCCCCCCCCu);
	n = ((n >>  4) & 0x0F0F0F0Fu) | ((n <<  4) & 0xF0F0F0F0u);
	n = ((n >>  8) & 0x00FF00FFu) | ((n <<  8) & 0xFF00FF00u);
	n = ((n >> 16) & 0x0000FFFFu) | ((n << 16) & 0xFFFF0000u);
	return n;
}

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

#if 1
	uint16_t compute_crc(const uint8_t *data, const unsigned int data_len)
	{	// using the reverse computation avoids having to reverse the bit order during and after
		unsigned int i;
		uint16_t crc = 0;
		for (i = 0; i < data_len; i++)
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
				//CRC_DATAIN = bit_reverse_8(data[i]);

			#else
				uint8_t mask;

				// bit reverse each data byte before adding it to the CRC
				// the cortex CPU might have an instruction to bit reverse for us ?
				//
				const uint8_t bits = reverse_bits(data[i], 8);
				//const uint8_t bits = bit_reverse_8(*data++);

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
	//	return bit_reverse_16(crc) ^ 0xffff;
	}
#endif

#define FEC_K   7

void error_correction(uint8_t *data)
{
	int     i;
	uint8_t shift_reg;
	uint8_t syn;

	for (i = 0, shift_reg = 0, syn = 0; i < FEC_K; i++)
	{
		const uint8_t bi = data[i];
		int bit_num;
		for (bit_num = 0; bit_num < 8; bit_num++)
		{
			uint8_t b;
			unsigned int k = 0;

			shift_reg = (shift_reg << 1) | ((bi >> bit_num) & 1u);
			b         = ((shift_reg >> 6) ^ (shift_reg >> 5) ^ (shift_reg >> 2) ^ (shift_reg >> 0)) & 1u;
			syn       = (syn << 1) | (((b ^ (data[i + FEC_K] >> bit_num)) & 1u) ? 1u : 0u);

			if (syn & 0x80) k++;
			if (syn & 0x20) k++;
			if (syn & 0x04) k++;
			if (syn & 0x02) k++;

			if (k >= 3)
			{	// correct bit error
				int ii = i;
				int bn = bit_num - 7;
				if (bn < 0)
				{
					bn += 8;
					ii--;
				}
				if (ii >= 0)
					data[ii] ^= 1u << bn;
				syn ^= 0xA6;   // 10100110
			}
		}
	}
}

uint8_t * decode_data(uint8_t *data)
{
	uint16_t crc1;
	uint16_t crc2;

//	(void)data;

	{	// de-interleave

		unsigned int i;
		unsigned int k;
		unsigned int m;
		uint8_t deinterleaved[(FEC_K * 2) * 8];

		// de-interleave the received bits
		for (i = 0, k = 0; i < 16; i++)
		{
			for (m = 0; m < FEC_K; m++)
			{
				const unsigned int n = (m * 16) + i;
				deinterleaved[k++] = (data[n >> 3] >> ((7 - n) & 7u)) & 1u;
			}
		}

		// copy the de-interleaved bits to the data buffer
		for (i = 0, m = 0; i < (FEC_K * 2); i++)
		{
			unsigned int k;
			uint8_t b = 0;
			for (k = 0; k < 8; k++)
				if (deinterleaved[m++])
					b |= 1u << k;
			data[i] = b;
		}
	}

	error_correction(data);

	crc1 = compute_crc(data, 4);
	crc2 = (data[5] << 8) | (data[4] << 0);

	if (crc1 != crc2)
		return NULL;

	// appears to be a valid packet



	// TODO: more stuff




	return NULL;
}

uint8_t * encode_data(uint8_t *data)
{
	// R=1/2 K=7 convolutional coder
	//
	// op     0x01
	// arg    0x80
	// id     0x1234
	// crc    0x2E3E
	// status 0x00
	// FEC    0x6580A862DD8808
	//
	// 01 80 1234 2E3E 00 6580A862DD8808
	//
	// 1. reverse the bit order for each byte of the first 7 bytes (to undo the reversal performed for display, above)
	// 2. feed those bits into a shift register which is preloaded with all zeros
	// 3. for each bit, calculate the modulo-2 sum: bit(n-0) + bit(n-2) + bit(n-5) + bit(n-6)
	// 4. then for each byte of resulting output, again reverse those bits to generate the values shown above

	{	// add the FEC bits to the end of the data
		unsigned int i;
		uint8_t shift_reg = 0;
		for (i = 0; i < FEC_K; i++)
		{
			unsigned int  bit_num;
			const uint8_t bi = data[i];
			uint8_t       bo = 0;
			for (bit_num = 0; bit_num < 8; bit_num++)
			{
				shift_reg = (shift_reg << 1) | ((bi >> bit_num) & 1u);
				bo |= (((shift_reg >> 6) ^ (shift_reg >> 5) ^ (shift_reg >> 2) ^ (shift_reg >> 0)) & 1u) << bit_num;
			}
			data[FEC_K + i] = bo;
		}
	}

	{	// interleave the bits

		unsigned int i;
		unsigned int k;
		unsigned int m;
		uint8_t interleaved[(FEC_K * 2) * 8];

		// bit interleaver
		for (i = 0, k = 0, m = 0; i < (FEC_K * 2); i++)
		{
			unsigned int bit_num;
			const uint8_t b = data[i];
			for (bit_num = 0; bit_num < 8; bit_num++)
			{
				interleaved[k] = (b >> bit_num) & 1u;
				k += 16;
				if (k >= sizeof(interleaved))
					k = ++m;
			}
		}

		// copy the interleaved bits back to the input/output buffer
		for (i = 0, k = 0; i < (FEC_K * 2); i++)
		{
			int bit_num;
			uint8_t b = 0;
			for (bit_num = 7; bit_num >= 0; bit_num--)
				if (interleaved[k++])
					b |= 1u << bit_num;
			data[i] = b;
		}
	}

	return data + (FEC_K * 2);
}

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
			if (b1 != b2)
				out |= 1u << bit_num;        // previous bit and new bit are different
			b1 = b2;
		}
		data[i] = out;
	}
}

unsigned int MDC1200_encode_single_packet(uint8_t *data, const uint8_t op, const uint8_t arg, const uint16_t unit_id)
{
	unsigned int size;
	uint16_t     crc;
	uint8_t     *p = data;

	memcpy(p, header, sizeof(header));
	p += sizeof(header);

	p[0] = op;
	p[1] = arg;
	p[2] = (unit_id >> 8) & 0x00ff;
	p[3] = (unit_id >> 0) & 0x00ff;
	crc = compute_crc(p, 4);
	p[4] = (crc >> 0) & 0x00ff;
	p[5] = (crc >> 8) & 0x00ff;
	p[6] = 0;      // unknown field (00 for PTTIDs, 76 for STS and MSG)

	p = encode_data(p);

#if 0
	{	// test packet
		//
		// op 0x01, arg 0x80, id 0xB183
		//
		const uint8_t test_packet[] = {0x07, 0x25, 0xDD, 0xD5, 0x9F, 0xC5, 0x3D, 0x89, 0x2D, 0xBD, 0x57, 0x35, 0xE7, 0x44};
		memcpy(data + sizeof(header), test_packet, sizeof(test_packet));
	}
#endif

	size = (unsigned int)(p - data);

	delta_modulation(data, size);

	return size;
//	return 26;
}

unsigned int MDC1200_encode_double_packet(uint8_t *data, const uint8_t op, const uint8_t arg, const uint16_t unit_id, const uint8_t b0, const uint8_t b1, const uint8_t b2, const uint8_t b3)
{
	unsigned int size;
	uint16_t     crc;
	uint8_t     *p = data;

	memcpy(p, header, sizeof(header));
	p += sizeof(header);

	p[0] = op;
	p[1] = arg;
	p[2] = (unit_id >> 8) & 0x00ff;
	p[3] = (unit_id >> 0) & 0x00ff;
	crc = compute_crc(p, 4);
	p[4] = (crc >> 0) & 0x00ff;
	p[5] = (crc >> 8) & 0x00ff;
	p[6] = 0;      // status byte

	p = encode_data(p);

	p[0] = b0;
	p[1] = b1;
	p[2] = b2;
	p[3] = b3;
	crc = compute_crc(p, 4);
	p[4] = (crc >> 0) & 0x00ff;
	p[5] = (crc >> 8) & 0x00ff;
	p[6] = 0;      // status byte

	p = encode_data(p);

	size = (unsigned int)(p - data);

	delta_modulation(data, size);

	return size;
//	return 40;
}
/*
void test(void)
{
	uint8_t data[14 + 14 + 5 + 7];

	const int size = MDC1200_encode_single_packet(data, 0x12, 0x34, 0x5678);

//	const int size = MDC1200_encode_double_packet(data, 0x55, 0x34, 0x5678, 0x0a, 0x0b, 0x0c, 0x0d);
}
*/