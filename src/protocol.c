#include "protocol.h"

#include "protocol_http.h"
#include "protocol_musicd.h"

protocol_t *protocols[] = { &protocol_http, &protocol_musicd, NULL };
