#pragma once
#include <nghttp2/nghttp2.h>
#include <string_view>
#include "parseCallbackCtx.hpp"



int headerCb(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags, void *user_data) {

        auto userData = static_cast<parseCallbackCtx*>(user_data);
        std::string_view name_view(reinterpret_cast<const char*>(name), namelen);
        std::string_view value_view(reinterpret_cast<const char*>(value), valuelen);

        printf("Header received: %.*s: %.*s\n", (int)namelen, name, (int)valuelen, value);

        if(name_view == ":status" and value_view != "200"){
            return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }

        
        return 0;
    }
