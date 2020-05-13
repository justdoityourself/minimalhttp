/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <algorithm>
#include <execution>

#include "../catch.hpp"
#include "../mio.hpp"

#include "tcp.hpp"
#include "http.hpp"
#include "ftp.hpp"
#include "iscsi.hpp"
#include "client.hpp"

#include "d8u/string_switch.hpp"
#include "d8u/string.hpp"

using namespace mhttp;
using namespace d8u;

#include "volrng/iscsi_win.hpp"

//Wireshark replay to compare and iron out bugs
void iscsi_replay(std::string_view file)
{
    iscsi::ISCSIConnection dummy;
    mio::mmap_source replay(file);

    Helper stream(replay);

    bool is_data_same;
    bool is_length_same;
    bool is_header_same;

    std::vector<uint8_t> buffer(100 * 1024 * 1024);
    size_t count = 0;

    while (stream.size() && *stream.data() != '\n')
    {
        count++;

        auto _read = stream.GetLine2();
        auto rcmd = _read.GetWord();

        if (rcmd != std::string_view("Read:"))
            std::cout << "Fix Read" << std::endl;

        auto read = d8u::util::to_bin(_read);
        stream.GetLine2();

        auto res = iscsi::ISCSIBaseCommands(dummy, gsl::span<uint8_t>(read.data(), read.size()), [&](auto& c) {}, [&](auto& c) {},[&](auto &c,uint64_t sector, uint32_t count, auto&& cb)
        {
            cb(sector, count, gsl::span<uint8_t>(buffer.data() + sector * 512, count * 512));
        }, [&](auto& c, uint64_t sector, uint32_t count, gsl::span<uint8_t> data)
        {
            std::copy(data.begin(), data.end(), buffer.begin() + sector * 512);
        }, [&](auto& c) {});

        if (!res)
            std::cout << "Not implemented" << std::endl;

        size_t count = 0;
        while (dummy.queue.size())
        {
            std::vector<uint8_t> response, payload;
            dummy.TryWrite(response);
            dummy.TryWrite(payload);

            if (payload.size())
                response.insert(response.end(), payload.begin(), payload.end());

            auto _write = stream.GetLine2();
            auto wcmd = _write.GetWord();

            if (wcmd != std::string_view("Write:"))
                std::cout << "Fix Write" << std::endl;

            auto write = d8u::util::to_bin(_write);
            stream.GetLine2();

            is_data_same = is_length_same = write.size() == response.size();
            if (is_length_same)
                is_data_same = std::equal(response.begin() + 48, response.end(), write.begin() + 48);
            is_header_same = std::equal(response.begin(), response.begin() + 48, write.begin());

            if (!is_header_same)
            {
                bool only_size_delta = true;
                for (size_t i = 0; i < 48; i++)
                {
                    if (i == 5 || i == 6 || i == 7)
                        continue; //Ignore Payload Differences

                    if (i > 24 && i < 36)
                        continue; //Ignore Sequencing

                    if (response[i] != write[i])
                    {
                        only_size_delta = false;
                        break;
                    }
                }

                if (!only_size_delta)
                {
                    std::cout << "Expected: " << std::string_view(_write.c_str(), 48 * 2) << std::endl;
                    std::cout << "Actual:   " << d8u::util::to_hex(gsl::span<uint8_t>(response.data(), 48)) << std::endl << std::endl;
                }
            }

            if (is_length_same && response.size() > 255);
            else if (!is_data_same)
            {
                std::cout << "Expected Data (STR): " << std::string_view((char*)(write.data() + 48), write.size() - 48) << std::endl;
                std::cout << "Actual Data (STR):   " << std::string_view((char*)(response.data() + 48), response.size() - 48) << std::endl << std::endl;

                std::cout << "Expected Data (HEX): " << d8u::util::to_hex(gsl::span<uint8_t>(write.data() + 48, write.size() - 48)) << std::endl;
                std::cout << "Actual Data (HEX):   " << d8u::util::to_hex(gsl::span<uint8_t>(response.data() + 48, response.size() - 48)) << std::endl << std::endl;
            }

            count++;
        }
    }
}

TEST_CASE("iscsi replay", "[mhttp::]")
{
    //iscsi_replay("create.txt");
    //iscsi_replay("initialize.txt");
    //iscsi_replay("format.txt");
    //iscsi_replay("filecopy.txt");

    //iscsi_replay("iscsi_test3.txt");
    //iscsi_replay("iscsi_test2.txt");
    //iscsi_replay("iscsi_test.txt");
}

TEST_CASE("iscsi custom disk", "[mhttp::]")
{
    std::vector<uint8_t> buffer(100 * 1024 * 1024);

    iscsi::basic_disk disk{ false,100 * 1024 * 1024 / 512, 512, "testtargetname100" };

    auto tcp = iscsi::make_iscsi_server("3260", [&](auto& c) 
    {
        if(c.target_name == "testtargetname100")
            c.disk = &disk;
    }, [&](auto& c) 
    {
        //LOGOUT
        c.disk = nullptr;
    }, [&](auto& c,uint64_t sector, uint32_t count, auto&& cb)
    {
        auto block = gsl::span<uint8_t>(buffer.data() + sector * 512, count * 512);

        //std::cout << "Read:" << sector << " " << count << std::endl;
        //std::cout << util::to_hex(block) << std::endl;

        cb(sector, count, block);
    }, [&](auto& c,uint64_t sector, uint32_t count, gsl::span<uint8_t> data)
    {
        //std::cout << "Write:" << sector << " " << count << std::endl;

        std::copy(data.begin(), data.end(), buffer.begin() + sector * 512);
    },[&](auto& c) {});

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    volrng::win::ISCSIClient::ResetPortal("127.0.0.1");
    volrng::win::ISCSIClient::LogoutAll("127.0.0.1");

    volrng::win::ISCSIClient::DirectLogin("testtargetname100", "127.0.0.1");

    std::this_thread::sleep_for(std::chrono::milliseconds(3000)); //Wait for disk to be assigned a legacy id

    volrng::win::ISCSIClient::EnumerateSessions("", [](auto & map)
    {
        if (map["Target Name"] == std::string_view("testtargetname100"))
        {
            std::cout << map["Legacy Device Name"] << std::endl;
            volrng::win::ISCSIClient::Partition(std::string_view(&map["Legacy Device Name"].back(),1), "Z");
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    CHECK(true == std::filesystem::copy_file("replay/replay.7z", "Z:/replay.7z"));

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    volrng::win::ISCSIClient::LogoutAll("127.0.0.1");
    volrng::win::ISCSIClient::ResetPortal("127.0.0.1");
}

TEST_CASE("iscsi basics", "[mhttp::]")
{
    std::vector<uint8_t> buffer(100 * 1024 * 1024);

    auto tcp = iscsi::make_iscsi_server("3260", [&](auto& c) {}, [&](auto& c) {}, [&](auto& c,uint64_t sector, uint32_t count, auto&& cb)
    {
        auto block = gsl::span<uint8_t>(buffer.data() + sector * 512, count * 512);

        //std::cout << "Read:" << sector << " " << count << std::endl;
        //std::cout << util::to_hex(block) << std::endl;

        cb(sector, count, block);
    }, [&](auto& c,uint64_t sector, uint32_t count, gsl::span<uint8_t> data)
    {
        //std::cout << "Write:" << sector << " " << count << std::endl;

        std::copy(data.begin(), data.end(), buffer.begin() + sector * 512);
    }, 
    [&](auto& c) {},
    {false,100 * 1024 * 1024 /512, 512, "RamDisk100"});

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    volrng::win::ISCSIClient::ResetPortal("127.0.0.1");
    volrng::win::ISCSIClient::LogoutAll("127.0.0.1");

    volrng::win::ISCSIClient::EnumerateTargets("", [](std::string_view target) 
    {
        std::cout << target << std::endl;
    });

    volrng::win::ISCSIClient::Login("", "ramdisk100");

    volrng::win::ISCSIClient::EnumerateMappings("", [](std::string_view session, std::string_view target, std::string_view device)
    {
        std::cout << session << " " << device << " " << target << std::endl;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(3000)); //Wait for disk to be assigned a legacy id

    volrng::win::ISCSIClient::EnumerateSessions("", [](auto & map)
    {
        if (map["Target Name"] == std::string_view("ramdisk100"))
        {
            std::cout << map["Legacy Device Name"] << std::endl;
            volrng::win::ISCSIClient::Partition(std::string_view(&map["Legacy Device Name"].back(),1), "Z");
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    CHECK(true == std::filesystem::copy_file("replay/replay.7z", "Z:/replay.7z"));

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    volrng::win::ISCSIClient::LogoutAll("127.0.0.1");
    volrng::win::ISCSIClient::ResetPortal("127.0.0.1");
}

TEST_CASE("ftp basics", "[mhttp::]")
{
    //NOTE this was tested manually, todo get ftp code client and automate
    auto tcp = ftp::make_ftp_server("127.0.0.1","8999","8777", [&](auto& c,std::string_view enum_path,auto cb)
    {
        //enumerate
        cb(true, 0, 0, "folder1");
        cb(true, 0, 0, "folder2");
        cb(true, 0, 0, "folder3");

        cb(false, 0, 0, "test.txt");
        cb(false, 0, 0, "test2.txt");
    }, [&](auto& c,std::string_view file, auto cb)
    {
        //request file stream
        cb(std::string_view("THIS IS A SIMPLE TEST FILE CONTENTS!"));
    }, [&](auto & c)
    {
        return true; //login Accept all
    },
    [&](auto& c)
    {
        //Logout
    });

    //MANUAL
    //std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 60 * 30));
}

TEST_CASE("Dropping Connections", "[mhttp::]")
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

    {
        MsgConnection c("127.0.0.1:8999");

        auto message = std::string_view("THIS IS A MESSAGE");
        auto result = c.Transact(message);

        CHECK(std::equal(message.begin(), message.end(), result.begin()));
    }

    {
        MsgConnection c("127.0.0.1:8999");

        auto message = std::string_view("THIS IS A MESSAGE");
        auto result = c.Transact(message);

        CHECK(std::equal(message.begin(), message.end(), result.begin()));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    CHECK(tcp.ConnectionCount() == 0);

    tcp.Shutdown();
}

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