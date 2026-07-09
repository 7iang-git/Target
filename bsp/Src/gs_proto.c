#include "gs_proto.h"

#include <string.h>

enum {
    GS_STAGE_WAIT_SOF1 = 0,
    GS_STAGE_WAIT_SOF2,
    GS_STAGE_READ_HEADER,
    GS_STAGE_READ_PAYLOAD,
    GS_STAGE_READ_CRC,
};

static inline uint16_t read_u16_le(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static inline int16_t read_s16_le(const uint8_t* p) {
    return (int16_t)read_u16_le(p);
}

static inline uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static inline void write_u16_le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static inline void write_s16_le(uint8_t* p, int16_t v) {
    write_u16_le(p, (uint16_t)v);
}

static inline void write_u32_le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void reset_frame_state(gs_parser_t* parser) {
    parser->stage = GS_STAGE_WAIT_SOF1;
    parser->header_pos = 0;
    parser->payload_pos = 0;
    parser->crc_pos = 0;
    parser->payload_len = 0;
}

void gs_parser_reset(gs_parser_t* parser) {
    if (!parser) {
        return;
    }
    reset_frame_state(parser);
    memset(&parser->stats, 0, sizeof(parser->stats));
}

void gs_parser_init(gs_parser_t* parser) {
    if (!parser) {
        return;
    }
    memset(parser, 0, sizeof(*parser));
    reset_frame_state(parser);
}

const gs_parser_stats_t* gs_parser_get_stats(const gs_parser_t* parser) {
    if (!parser) {
        return NULL;
    }
    return &parser->stats;
}

uint16_t gs_crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFFu;
    size_t i;

    if (!data) {
        return crc;
    }

    for (i = 0; i < len; i++) {
        uint8_t b;
        crc ^= (uint16_t)data[i] << 8;
        for (b = 0; b < 8; b++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static int parse_state_payload(const gs_parser_t* parser, gs_state_packet_t* out_pkt) {
    const uint8_t* p;
    if (!parser || !out_pkt) {
        return -1;
    }

    p = parser->payload;

    out_pkt->seq = parser->header[3];
    out_pkt->id = p[0];
    out_pkt->lx = read_s16_le(&p[1]);
    out_pkt->ly = read_s16_le(&p[3]);
    out_pkt->rx = read_s16_le(&p[5]);
    out_pkt->ry = read_s16_le(&p[7]);
    out_pkt->btn = read_u32_le(&p[9]);
    out_pkt->lt = read_u16_le(&p[13]);
    out_pkt->rt = read_u16_le(&p[15]);
    out_pkt->dpad = p[17];

    return 0;
}

int gs_parse_rumble_payload(uint8_t seq,
                            const uint8_t* payload,
                            size_t payload_len,
                            gs_rumble_cmd_t* out_cmd) {
    if (!payload || !out_cmd) {
        return -1;
    }
    if (payload_len != GS_RUMBLE_PAYLOAD_LEN) {
        return -1;
    }

    out_cmd->seq = seq;
    out_cmd->id = payload[0];
    out_cmd->ms = read_u16_le(&payload[1]);
    out_cmd->weak = payload[3];
    out_cmd->strong = payload[4];
    return 0;
}

int gs_parser_feed_byte(gs_parser_t* parser, uint8_t byte_in, gs_state_packet_t* out_pkt) {
    uint16_t rx_crc;
    uint16_t calc_crc;
    uint8_t crc_src[4 + GS_MAX_PAYLOAD_LEN];

    if (!parser) {
        return -3;
    }

    parser->stats.bytes_in++;

    switch (parser->stage) {
        case GS_STAGE_WAIT_SOF1:
            if (byte_in == GS_SOF1) {
                parser->stage = GS_STAGE_WAIT_SOF2;
            }
            return 0;

        case GS_STAGE_WAIT_SOF2:
            if (byte_in == GS_SOF2) {
                parser->stage = GS_STAGE_READ_HEADER;
                parser->header_pos = 0;
                parser->payload_pos = 0;
                parser->crc_pos = 0;
            } else if (byte_in == GS_SOF1) {
                // 可能是新的 SOF1，继续等待 SOF2
                parser->stage = GS_STAGE_WAIT_SOF2;
            } else {
                parser->stage = GS_STAGE_WAIT_SOF1;
            }
            return 0;

        case GS_STAGE_READ_HEADER:
            parser->header[parser->header_pos++] = byte_in;
            if (parser->header_pos < 4u) {
                return 0;
            }

            parser->payload_len = parser->header[2];
            if (parser->payload_len > GS_MAX_PAYLOAD_LEN) {
                parser->stats.frames_length_error++;
                reset_frame_state(parser);
                return -3;
            }

            parser->stage = GS_STAGE_READ_PAYLOAD;
            return 0;

        case GS_STAGE_READ_PAYLOAD:
            if (parser->payload_pos >= parser->payload_len) {
                parser->stats.frames_length_error++;
                reset_frame_state(parser);
                return -3;
            }

            parser->payload[parser->payload_pos++] = byte_in;
            if (parser->payload_pos == parser->payload_len) {
                parser->stage = GS_STAGE_READ_CRC;
                parser->crc_pos = 0;
            }
            return 0;

        case GS_STAGE_READ_CRC:
            parser->crc_le[parser->crc_pos++] = byte_in;
            if (parser->crc_pos < 2u) {
                return 0;
            }

            // 一帧接收完成：对 头部+负载 做 CRC 校验
            memcpy(crc_src, parser->header, 4u);
            if (parser->payload_len > 0u) {
                memcpy(crc_src + 4u, parser->payload, parser->payload_len);
            }
            calc_crc = gs_crc16_ccitt(crc_src, 4u + parser->payload_len);
            rx_crc = read_u16_le(parser->crc_le);

            if (rx_crc != calc_crc) {
                parser->stats.frames_crc_error++;
                reset_frame_state(parser);
                return -1;
            }

            if (parser->header[0] != GS_PROTO_VERSION) {
                parser->stats.frames_version_error++;
                reset_frame_state(parser);
                return -2;
            }

            if (parser->header[1] != GS_FRAME_TYPE_STATE || parser->payload_len != GS_STATE_PAYLOAD_LEN) {
                parser->stats.frames_type_error++;
                reset_frame_state(parser);
                return -2;
            }

            if (parse_state_payload(parser, out_pkt) != 0) {
                parser->stats.frames_type_error++;
                reset_frame_state(parser);
                return -2;
            }

            parser->stats.frames_ok++;
            reset_frame_state(parser);
            return 1;

        default:
            reset_frame_state(parser);
            return -3;
    }
}

size_t gs_parser_feed(gs_parser_t* parser,
                      const uint8_t* data,
                      size_t len,
                      gs_state_packet_t* out_packets,
                      size_t out_cap) {
    size_t i;
    size_t produced = 0u;
    gs_state_packet_t sink;

    if (!parser || !data) {
        return 0u;
    }

    for (i = 0u; i < len; i++) {
        gs_state_packet_t* dst = &sink;
        int rc;

        if (out_packets && produced < out_cap) {
            dst = &out_packets[produced];
        }

        rc = gs_parser_feed_byte(parser, data[i], dst);
        if (rc == 1) {
            if (out_packets && produced < out_cap) {
                produced++;
            } else {
                parser->stats.frames_output_overflow++;
            }
        }
    }

    return produced;
}

int gs_build_state_frame(const gs_state_packet_t* pkt, uint8_t out_frame[GS_STATE_FRAME_SIZE]) {
    uint16_t crc;

    if (!pkt || !out_frame) {
        return -1;
    }

    out_frame[0] = GS_SOF1;
    out_frame[1] = GS_SOF2;
    out_frame[2] = GS_PROTO_VERSION;
    out_frame[3] = GS_FRAME_TYPE_STATE;
    out_frame[4] = GS_STATE_PAYLOAD_LEN;
    out_frame[5] = pkt->seq;

    // 负载位于 [6..23]
    out_frame[6] = pkt->id;
    write_s16_le(&out_frame[7], pkt->lx);
    write_s16_le(&out_frame[9], pkt->ly);
    write_s16_le(&out_frame[11], pkt->rx);
    write_s16_le(&out_frame[13], pkt->ry);
    write_u32_le(&out_frame[15], pkt->btn);
    write_u16_le(&out_frame[19], pkt->lt);
    write_u16_le(&out_frame[21], pkt->rt);
    out_frame[23] = pkt->dpad;

    crc = gs_crc16_ccitt(&out_frame[2], 4u + GS_STATE_PAYLOAD_LEN);
    write_u16_le(&out_frame[24], crc);

    return 0;
}

int gs_build_rumble_frame(const gs_rumble_cmd_t* cmd, uint8_t out_frame[GS_RUMBLE_FRAME_SIZE]) {
    uint16_t crc;

    if (!cmd || !out_frame) {
        return -1;
    }

    out_frame[0] = GS_SOF1;
    out_frame[1] = GS_SOF2;
    out_frame[2] = GS_PROTO_VERSION;
    out_frame[3] = GS_FRAME_TYPE_RUMBLE_CMD;
    out_frame[4] = GS_RUMBLE_PAYLOAD_LEN;
    out_frame[5] = cmd->seq;

    // 负载位于 [6..10]
    out_frame[6] = cmd->id;
    write_u16_le(&out_frame[7], cmd->ms);
    out_frame[9] = cmd->weak;
    out_frame[10] = cmd->strong;

    crc = gs_crc16_ccitt(&out_frame[2], 4u + GS_RUMBLE_PAYLOAD_LEN);
    write_u16_le(&out_frame[11], crc);
    return 0;
}
