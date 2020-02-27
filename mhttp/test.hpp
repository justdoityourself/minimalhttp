/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "../catch.hpp"

#include "tcp.hpp"
#include "http.hpp"

#include "d8u/string_switch.hpp"

using namespace mhttp;
using namespace d8u;

TEST_CASE("Simple TCP", "[mhttp::]")
{
    TcpConnection c("127.0.0.1:444");
}

TEST_CASE("Simple Http Server", "[mhttp::]")
{
    auto & http = make_http_server("8032",[&](auto & c, const auto& request)
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
        Threads().Delay(3000);

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