#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H
#include "zf_common_headfile.h"
#include "hmac_sha256.h" 
#include "base64.h"  


// url缂栫爜
void url_encode(const char* str, char* encoded);
// 杩炴帴鍒� WebSocket 鏈嶅姟鍣�
bool websocket_client_connect(const char* url);
// 鍙戦�佹暟鎹埌 WebSocket 鏈嶅姟鍣�
bool websocket_client_send(const uint8_t* data, uint32_t len);
// 鎺ユ敹鏁版嵁浠� WebSocket 鏈嶅姟鍣�
bool websocket_client_receive(uint8_t* buffer);
// 鍏抽棴 WebSocket 杩炴帴
void websocket_client_close();
// 灏佽 WebSocket 鏁版嵁甯�
uint32_t websocket_create_frame(uint8_t* frame, const uint8_t* payload, uint64_t payload_len, uint8_t type, bool mask);

#endif // WEBSOCKET_CLIENT_H
