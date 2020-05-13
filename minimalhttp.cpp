/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#ifdef TEST_RUNNER


#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "mhttp/test.hpp"

int main(int argc, char* argv[])
{
    return Catch::Session().run(argc, argv);
}


#endif //TEST_RUNNER



#if ! defined(TEST_RUNNER)

#include <iostream>
#include <string>
#include "clipp.h"

using namespace clipp;

int main(int argc, char* argv[])
{
    bool mount = false, dismount = false, step = false, validate = false;
    std::string path = "test";
    int size = 1;

    auto cli = (
        opt_value("test directory", path),
        opt_value("size", size),
        option("-m", "--mount").set(mount).doc("Mount the test volume ( path )"),
        option("-s", "--step").set(step).doc("Mutate the test data"),
        option("-v", "--validate").set(validate).doc("Validate test metadata against path ( path )"),
        option("-d", "--dismount").set(dismount).doc("Dismount the test data")
        );

    try
    {
        if (!parse(argc, argv, cli)) cout << make_man_page(cli, argv[0]);
        else
        {
        }
    }
    catch (const exception & ex)
    {
        std::cerr << ex.what() << endl;
        return -1;
    }

    return 0;
}


#endif //! defined(BENCHMARK_RUNNER) && ! defined(TEST_RUNNER)


