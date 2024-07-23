/**
 * @file packetizer.h
 * @author K.Fukushima@nnct (21327@g.nagano-nct.ac.jp)
 * @brief 型を問わずバイト列に詰めるライブラリ　エンディアンを考慮
 * @version 0.1
 * @date 2024-04-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#pragma once
#include <stdint.h>
#include <string.h>
#if __has_include(<machine/_endian.h>)
#define LittleEndian (BYTE_ORDER == LITTLE_ENDIAN)
#include <sys/param.h>
#else
#define LittleEndian 1
#endif
//パケットを取り込んで、順次処理をおこなう。
struct packet_t {
    packet_t() : id(0),length(0) {
        memset(data, 0, 8);
    }
    uint16_t id;
    uint8_t data[8];
    uint8_t length;
};
class Packetizer
{
public:
    enum error_t
    {
        ERR_BUFFER_OVERFLOW,
        ERR_INVALID_LENGTH
    };
    Packetizer &setErrCallback(void (*callback)(error_t err))
    {
        errcallback = callback;
        return *this;
    };
    Packetizer(){

    };
    ~Packetizer(){

    };
    Packetizer &init(packet_t& packet,bool clear=false)
    {
        error=false;
        packet_=&packet;
        if(clear)memset(packet_->data, 0, 8);
        packet_->length = 0;
        // packet_->id = 0;
        unpackIndex = 0;
        return *this;
    };
    Packetizer &set(const uint8_t *data, uint8_t length)
    {
        if(error)return *this;
        memcpy(packet_->data, data, length);
        packet_->length = length;
        return *this;
    };

    template <typename T>
    Packetizer &pack(T data)
    {
        if(error)return *this;
#if LittleEndian
        if (packet_->length + sizeof(T) > 8)
        {
            errcallback(ERR_BUFFER_OVERFLOW);
            error=true;
            return *this;
        }
        memcpy(packet_->data + packet_->length, &data, sizeof(T));
#else
        if (packet_->length + sizeof(T) > 8)
        {
            errcallback(ERR_BUFFER_OVERFLOW);
            return *this;
        }
        for (int i = 0; i < sizeof(T); i++)
        {
            packet_->data[packet_->length + i] = ((uint8_t *)&data)[sizeof(T) - i - 1];
        }
#endif
        packet_->length += sizeof(T);
        return *this;
    };

    Packetizer &pack(uint8_t *data, uint8_t length)
    {
        if(error)return *this;
        if (packet_->length + length > 8)
        {
            errcallback(ERR_BUFFER_OVERFLOW);
            error=true;
            return *this;
        }
        memcpy(packet_->data + packet_->length, data, length);
        packet_->length += length;
        return *this;
    };

    template <typename T>
    Packetizer &unpack(T &data)
    {
        if(error)return *this;
        if (unpackIndex + sizeof(T) > 8)
        {
            errcallback(ERR_INVALID_LENGTH);
            return *this;
        }
#if LittleEndian
        memcpy(&data, packet_->data + unpackIndex, sizeof(T));
        unpackIndex += sizeof(T);
#else
        for (int i = 0; i < sizeof(T); i++)
        {
            ((uint8_t *)&data)[i] = packet_->data[unpackIndex + sizeof(T) - i - 1];
        }
        unpackIndex += sizeof(T);
#endif
        return *this;
    };
    Packetizer &unpack(uint8_t *data, uint8_t length)
    {
        if(error)return *this;
        if (unpackIndex + length > 8)
        {
            errcallback(ERR_INVALID_LENGTH);
            return *this;
        }
        memcpy(data, packet_->data + unpackIndex, length);
        unpackIndex += length;
        return *this;
    };
    bool success(){
        return !error;
    }
private:
    packet_t* packet_=nullptr;
    uint8_t unpackIndex;
    void (*errcallback)(error_t err);
    bool error=true;
};
