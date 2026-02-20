#pragma once
#include    <unordered_map>
#include    <functional>
#include    <iostream>
#include    "message.h"

class CMessageDispacher {
private:
    std::unordered_map<MessageType, std::function<void(std::unique_ptr<MsgData>, uint32_t, uint16_t)>> m_handlers;

public:
    void registerHandler(MessageType type, std::function<void(std::unique_ptr<MsgData>, uint32_t, uint16_t)> handler) {
        m_handlers[type] = handler;
    }

    void dispatch(std::unique_ptr<MsgData> msg,uint32_t ip,uint16_t port) {
        if (m_handlers.find(msg->Msg.Header.type) != m_handlers.end()) {
            m_handlers[msg->Msg.Header.type](std::move(msg),ip,port);
        }
        else {
            std::cerr << "No handler registered for this message type." << std::endl;
        }
    }
};
