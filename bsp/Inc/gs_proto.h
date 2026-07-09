#ifndef GS_PROTO_H
#define GS_PROTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 协议常量
#define GS_SOF1 ((uint8_t)0xA5)
#define GS_SOF2 ((uint8_t)0x5A)
#define GS_PROTO_VERSION ((uint8_t)0x01)
#define GS_FRAME_TYPE_STATE ((uint8_t)0x01)
#define GS_FRAME_TYPE_RUMBLE_CMD ((uint8_t)0x11)

// 负载长度
#define GS_STATE_PAYLOAD_LEN ((uint8_t)18)
#define GS_RUMBLE_PAYLOAD_LEN ((uint8_t)5)
#define GS_MAX_PAYLOAD_LEN ((uint8_t)64)

// 2(SOF) + 4(头部: ver/type/len/seq) + payload + 2(CRC)
#define GS_STATE_FRAME_SIZE ((size_t)(2 + 4 + GS_STATE_PAYLOAD_LEN + 2))
#define GS_RUMBLE_FRAME_SIZE ((size_t)(2 + 4 + GS_RUMBLE_PAYLOAD_LEN + 2))
#define GS_MAX_FRAME_SIZE ((size_t)(2 + 4 + GS_MAX_PAYLOAD_LEN + 2))

typedef struct {
    uint8_t seq;
    uint8_t id;

    int16_t lx;
    int16_t ly;
    int16_t rx;
    int16_t ry;

    uint32_t btn;
    uint16_t lt;
    uint16_t rt;
    uint8_t dpad;
} gs_state_packet_t;

typedef struct {
    uint8_t seq;
    uint8_t id;
    uint16_t ms;
    uint8_t weak;
    uint8_t strong;
} gs_rumble_cmd_t;

typedef struct {
    uint32_t bytes_in;
    uint32_t frames_ok;
    uint32_t frames_crc_error;
    uint32_t frames_length_error;
    uint32_t frames_version_error;
    uint32_t frames_type_error;
    uint32_t frames_output_overflow;
} gs_parser_stats_t;

typedef struct {
    uint8_t stage;

    uint8_t header[4];
    uint8_t payload[GS_MAX_PAYLOAD_LEN];
    uint8_t crc_le[2];

    uint8_t header_pos;
    uint8_t payload_pos;
    uint8_t crc_pos;
    uint8_t payload_len;

    gs_parser_stats_t stats;
} gs_parser_t;

// gs_parser_feed_byte() 返回值说明
//  1: 成功解出一帧并写入 out_pkt
//  0: 当前还没有完整帧
// -1: 收到完整帧但 CRC 校验失败
// -2: 收到完整帧但版本/类型/格式不支持
// -3: 头部或长度非法，当前帧被丢弃
int gs_parser_feed_byte(gs_parser_t* parser, uint8_t byte_in, gs_state_packet_t* out_pkt);

// 批量喂入字节，返回写入 out_packets 的包数量。
// 如果解析出的包数量超过 out_cap，超出的包会丢弃并计入统计。
size_t gs_parser_feed(gs_parser_t* parser,
                      const uint8_t* data,
                      size_t len,
                      gs_state_packet_t* out_packets,
                      size_t out_cap);

void gs_parser_init(gs_parser_t* parser);
void gs_parser_reset(gs_parser_t* parser);

const gs_parser_stats_t* gs_parser_get_stats(const gs_parser_t* parser);

// CRC16-CCITT（多项式 0x1021，初值 0xFFFF）
uint16_t gs_crc16_ccitt(const uint8_t* data, size_t len);

// 可选辅助函数：构造一帧带有效 CRC 的状态帧。
// 成功返回 0，参数非法返回非 0。
int gs_build_state_frame(const gs_state_packet_t* pkt, uint8_t out_frame[GS_STATE_FRAME_SIZE]);

// 解析振动命令 payload（type=0x11, len=5）。
// 成功返回 0，参数非法或长度错误返回非 0。
int gs_parse_rumble_payload(uint8_t seq,
                            const uint8_t* payload,
                            size_t payload_len,
                            gs_rumble_cmd_t* out_cmd);

// 构造振动命令帧（type=0x11, len=5）。
// 成功返回 0，参数非法返回非 0。
int gs_build_rumble_frame(const gs_rumble_cmd_t* cmd, uint8_t out_frame[GS_RUMBLE_FRAME_SIZE]);

#ifdef __cplusplus
}
#endif

#endif  // 头文件保护 GS_PROTO_H
