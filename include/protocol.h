#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define CLIPD_SOCKET_PATH "/clipd.sock"
#define CLIPD_UID 0x7236D9D9

typedef enum
{
    STATUS_OK = 200,
    STATUS_ERR = 500,
    CMD_ADD_CLIP = 1,
    CMD_LIST_CLIPS = 2,
    CMD_GET_CLIP = 3,
    CMD_PASTE_CLIP = 4

} clip_cmd_t;

typedef struct __attribute__((packed))
{
    uint32_t uid; // Identifier for this process
    uint32_t command;
    uint32_t payload_len; // Byte length of the raw data
    uint32_t clip_id;     // ID used for GET requests

} ipc_header_t;

#endif