#pragma once
#include <nghttp2/nghttp2.h>
#include <string_view>
#include "parseCallbackCtx.hpp"



int headerCb(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags, void *user_data) {

        auto userData = static_cast<parseCallbackCtx*>(user_data);
        std::string_view name_view(reinterpret_cast<const char*>(name), namelen);
        std::string_view value_view(reinterpret_cast<const char*>(value), valuelen);

        
        if(name_view == ":status" and value_view == "200"){
            return 0;
        }else{
            auto t = nghttp2_session_get_stream_user_data(session, frame->hd.stream_id);
            delete static_cast<parseCallbackCtx*>(t);

            //does it kill the whole stream? 
            return -1;
        }

        

    }
