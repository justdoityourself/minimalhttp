/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <algorithm>
#include <execution>

#include "../catch.hpp"

#include "tcp.hpp"
#include "http.hpp"
#include "client.hpp"

#include "d8u/string_switch.hpp"

using namespace mhttp;
using namespace d8u;

TEST_CASE("Threaded Client Async", "[mhttp::]")
{
    constexpr auto lim = 100;

    static std::array<std::string, lim> buffers;

    size_t msgc = 0;
    for (auto& s : buffers)
        s = std::to_string(msgc++) + "THIS IS A MESSAGE";

    auto tcp = make_halfmap_server("8993", [&](auto& c, auto header, auto reply)
    {
        uint32_t dx = *(uint32_t*)header.data();
        c.ActivateMap(reply, buffers[dx]);
    }, 4);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    {
        std::atomic<size_t> valid = 0;
        std::atomic<size_t> reads = 0;

        EventClientT< BufferedConnection<MsgConnection> > c("127.0.0.1:8993", ConnectionType::message);

        uint32_t dx = 0;
        for (auto& s : buffers)
            c.AsyncWriteCallbackT(dx++, [&](auto result,auto b) 
            {
                static auto ix = 0;

                if (std::equal(result.begin(), result.end(), buffers[ix++].begin()))
                    valid++;

                reads++;
            });

        while (reads < dx)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        CHECK(valid == lim);
    }

    tcp.Shutdown();
}

TEST_CASE("Threaded Client Async Reads", "[mhttp::]")
{
    constexpr auto lim = 100;

    static std::array<std::string, lim> buffers;

    size_t msgc = 0;
    for (auto& s : buffers)
        s = std::to_string(msgc++) + "THIS IS A MESSAGE";

    auto tcp = make_halfmap_server("8993", [&](auto& c, auto header, auto reply)
    {
        uint32_t dx = *(uint32_t*)header.data();
        c.ActivateMap(reply, buffers[dx]);
    }, 4);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    {
        std::atomic<size_t> valid = 0;
        std::atomic<size_t> reads = 0;

        ThreadedClientT < BufferedConnection<MsgConnection> > c("127.0.0.1:8993", ConnectionType::message, 
            [&](auto result, auto b)
            {
                static auto dx = 0;

                if (std::equal(result.begin(), result.end(), buffers[dx++].begin()))
                    valid++;

                reads++;
            },false);

        uint32_t dx = 0;
        for (auto& s : buffers)
            c.SendT(dx++);

        while(reads < dx)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        CHECK(valid == lim);
    }

    tcp.Shutdown();
}


TEST_CASE("Threaded MultiPlex HALFMAP", "[mhttp::]")
{
    constexpr auto lim = 100;

    static std::array<std::string, lim> buffers;

    size_t msgc = 0;
    for (auto& s : buffers)
        s = std::to_string(msgc++) + "THIS IS A MESSAGE";

    auto tcp = make_halfmap_server("8993", [&](auto& c, auto header, auto reply)
    {
        uint32_t dx = *(uint32_t*)header.data();
        c.ActivateMap(reply,buffers[dx]);
    }, 4);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::atomic<size_t> valid = 0;

    {
        MsgConnection c("127.0.0.1:8993");

        std::thread writer([&]() 
        {
            uint32_t dx = 0;
            for (auto& s : buffers)
                c.SendT(dx++);
        });

        std::thread reader([&]()
        {
            uint32_t dx = 0;
            for (auto& s : buffers)
            {
                auto result = c.ReceiveMessage();

                if (std::equal(result.begin(), result.end(), buffers[dx++].begin())) 
                    valid++;
            }
        });

        writer.join();
        reader.join();
    }

    CHECK(valid == lim);

    tcp.Shutdown();
}

TEST_CASE("Simple TCP", "[mhttp::]")
{
    auto tcp = make_tcp_server("8999", [&](auto& c, auto message, auto wr)
    {
        c.AsyncWrite(std::move(message));
    });

    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        MsgConnection c("127.0.0.1:8999");

        auto message = std::string_view("THIS IS A MESSAGE");
        auto result = c.Transact(message);

        CHECK(std::equal(message.begin(), message.end(), result.begin()));
    }


    tcp.Shutdown();
}

TEST_CASE("Simple Http Server", "[mhttp::]")
{
    auto http = make_http_server("8032",[&](auto & c, auto request, auto wr)
    {
        switch(switch_t(request.type))
        {
        default:
            c.Http400();
            break;
        case switch_t("GET"):
        case switch_t("Get"):
        case switch_t("get"):

            switch (switch_t(request.path))
            {
            default:
                c.Http400();
                break;
            case switch_t("/is"):
                c.Http200();
                break;
            case switch_t("/many"):
                c.Http200();
                break;
            case switch_t("/read"):
                c.Response("200 OK", std::string_view("CONTENTS"));
                break;
            }

            break;
        case switch_t("POST"):
        case switch_t("Post"):
        case switch_t("post"):

            switch (switch_t(request.path))
            {
            default:
                c.Http400();
                break;
            case switch_t("/write"):
                c.Response("200 OK", request.body);
                break;
            }
        }
    });

    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        HttpConnection c("127.0.0.1:8032");

        CHECK(c.Get("/is?1=testest").status == 200);
        CHECK(c.Get("/bad").status == 400);

        CHECK(c.Get("/many?1=testest&2=kkk&3=agdr").status == 200);

        auto read = c.Get("/read");

        auto expected = std::string_view("CONTENTS");

        CHECK( std::equal(expected.begin(), expected.end(), read.body.begin() ) );
        
        CHECK(read.status == 200);


        auto write = std::string_view("WRITEWRITEWRITE");

        auto wrep = c.Post("/write", write);

        CHECK(std::equal(write.begin(), write.end(), wrep.body.begin()));

        CHECK(wrep.status == 200);
    }

    http.Shutdown();
}

TEST_CASE("Threaded Non-multiplexed TCP", "[mhttp::]")
{
    constexpr auto lim = 100;

    auto tcp = make_tcp_server("8999", [&](auto& c, auto message, auto wr)
    {
        c.AsyncWrite(std::move(message));
    },4);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::array<std::string, lim> buffers;

    size_t msgc = 0;
    for(auto & s : buffers)
        s = std::to_string(msgc++) +"THIS IS A MESSAGE";

    std::atomic<size_t> valid = 0;

    std::for_each(std::execution::par, buffers.begin(), buffers.end(), [&](auto message)
    {
        MsgConnection c("127.0.0.1:8999");

        auto result = c.Transact(message);

        if (std::equal(message.begin(), message.end(), result.begin())) valid++;
    });

    CHECK(valid == lim);

    tcp.Shutdown();
}


TEST_CASE("Threaded Http Server", "[mhttp::]")
{
    constexpr auto lim = 100;

    auto http = make_http_server("8036", [&](auto& c, auto request, auto wr)
    {
        switch (switch_t(request.type))
        {
        default:
            c.Http400();
            break;
        case switch_t("GET"):
        case switch_t("Get"):
        case switch_t("get"):

            switch (switch_t(request.path))
            {
            default:
                c.Http400();
                break;
            case switch_t("/echo"):
                c.Response("200 OK", request.body);
                break;
            }

            break;
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::array<std::string, lim> buffers;

    size_t msgc = 0;
    for (auto& s : buffers)
        s = std::to_string(msgc++) + "THIS IS A MESSAGE";

    std::atomic<size_t> valid = 0;

    std::for_each(std::execution::par, buffers.begin(), buffers.end(), [&](auto message)
    {
        HttpConnection c("127.0.0.1:8036");

        auto rep = c.Request("GET","/echo",message);

        if(rep.status == 200) 
            valid++;

        if (std::equal(message.begin(), message.end(), rep.body.begin())) 
            valid++;
    });

    CHECK(valid == lim*2);


    http.Shutdown();
}

TEST_CASE("Threaded Non-multiplexed MAP", "[mhttp::]")
{
    constexpr auto lim = 100;

    std::mutex lk; //We have to simulate a global memory object
    std::list < std::vector<uint8_t > > _global;

    auto tcp = make_map_server("8993", [&](auto& c, auto header, auto wr)
    {
        auto [size, id] = Map32::DecodeHeader(header);
        std::vector<uint8_t>* ptarget;
        {
            std::lock_guard<std::mutex> lock(lk);
            _global.emplace_back(size);
            ptarget = &_global.back();
        }

        c.Read(*ptarget);
        c.AsyncMap(*ptarget);
    }, 4);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::array<std::string, lim> buffers;

    size_t msgc = 0;
    for (auto& s : buffers)
        s = std::to_string(msgc++) + "THIS IS A MESSAGE";

    std::atomic<size_t> valid = 0;

    std::for_each(std::execution::par, buffers.begin(), buffers.end(), [&](auto message)
    {
        MsgConnection c("127.0.0.1:8993");

        auto result = c.Transact32(std::string_view("ffffffffffffffffffffffffffffffff"),message);

        if (std::equal(message.begin(), message.end(), result.begin())) valid++;
    });

    CHECK(valid == lim);

    tcp.Shutdown();
}