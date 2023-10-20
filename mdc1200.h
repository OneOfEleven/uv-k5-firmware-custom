
#ifndef MDC1200H
#define MDC1200H

#include <stdint.h>

// 0x01 (0x80) is PTT ID
// 0x01 (0x00) is POST ID
// 0x11 (0x8A) is REMOTE MONITOR
// 0x22 (0x06) is STATUS REQ
// 0x2B (0x0C) is RADIO ENABLE
// 0x2B (0x00) is RADIO DISABLE
// 0x35 (0x89) is CALL ALERT
// 0x46 (0xXX) is STS XX
// 0x47 (0xXX) is MSG XX
// 0x63 (0x85) is RADIO CHECK

enum mdc1200_op_code_e {
	MDC1200_OP_CODE_PTT_ID         = 0x01,
	MDC1200_OP_CODE_POST_ID        = 0x01,
	MDC1200_OP_CODE_REMOTE_MONITOR = 0x11,
	MDC1200_OP_CODE_STATUS_REQ     = 0x22,
	MDC1200_OP_CODE_RADIO_ENABLE   = 0x2B,
	MDC1200_OP_CODE_RADIO_DISABLE  = 0x2B,
	MDC1200_OP_CODE_CALL_ALERT     = 0x35,
	MDC1200_OP_CODE_STS_XX         = 0x46,
	MDC1200_OP_CODE_MSG_XX         = 0x47,
	MDC1200_OP_CODE_RADIO_CHECK    = 0x63
};

unsigned int MDC1200_encode_single_packet(uint8_t *data, const uint8_t op, const uint8_t arg, const uint16_t unit_id);
unsigned int MDC1200_encode_double_packet(uint8_t *data, const uint8_t op, const uint8_t arg, const uint16_t unit_id, const uint8_t b0, const uint8_t b1, const uint8_t b2, const uint8_t b3);

#endif
