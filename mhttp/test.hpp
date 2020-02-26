/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "../catch.hpp"

#include "tcp.hpp"
#include "tcpserver.hpp"

using namespace mhttp;

TEST_CASE("Simple TCP", "[mhttp::]")
{
    TcpConnection c("127.0.0.1:444");
}

TEST_CASE("Simple Http Server", "[mhttp::]")
{
    make_http_server("8032",[&](const auto & c, const auto& request, const auto& body)
    {
        std::cout << "here";
    });
}