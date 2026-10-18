#include "jtml/local_server_security.h"
#include <gtest/gtest.h>

TEST(LocalServerSecurity, RejectsUntrustedAuthoritiesAndOrigins) {
    using JTMLInterpreter::localServerRequestAllowed;
    for (const auto* host : {"", "example.com:8000", "localhost.evil:8000", "localhost.:8000",
                            "127.0.0.2:8000", "localhost:8001", "localhost:8000@evil", "localhost:8000,evil"}) {
        EXPECT_FALSE(localServerRequestAllowed(host, "", false, "", 8000, 8000)) << host;
    }
    for (const auto* origin : {"", "null", "https://localhost:8000", "http://localhost:8001",
                              "http://localhost.evil:8000", "http://localhost:8000/", "http://localhost:8000@evil",
                              "http://localhost:8000,http://evil", "file://"}) {
        EXPECT_FALSE(localServerRequestAllowed("localhost:8000", origin, true, "", 8000, 8000)) << origin;
        EXPECT_FALSE(localServerRequestAllowed("localhost:8080", origin, true, "", 8080, 8000)) << origin;
    }
    EXPECT_FALSE(localServerRequestAllowed("localhost:8000", "http://127.0.0.1:8000", true, "", 8000, 8000));
    EXPECT_FALSE(localServerRequestAllowed("localhost:8000", "", false, "same-site", 8000, 8000));
    EXPECT_FALSE(localServerRequestAllowed("localhost:8080", "", false, "cross-site", 8080, 8000));
}

TEST(LocalServerSecurity, AllowsNativeClientsAndPairedBrowserOrigins) {
    using JTMLInterpreter::localServerRequestAllowed;
    EXPECT_TRUE(localServerRequestAllowed("localhost:8000", "", false, "", 8000, 8000));
    EXPECT_TRUE(localServerRequestAllowed("127.0.0.1:8000", "http://127.0.0.1:8000", true, "same-origin", 8000, 8000));
    EXPECT_TRUE(localServerRequestAllowed("LOCALHOST:8000", "http://localhost:8000", true, "same-origin", 8000, 8000));
    EXPECT_TRUE(localServerRequestAllowed("localhost:8080", "http://localhost:8000", true, "same-site", 8080, 8000));
    EXPECT_TRUE(localServerRequestAllowed("localhost:8080", "http://127.0.0.1:8000", true, "cross-site", 8080, 8000));
    EXPECT_TRUE(localServerRequestAllowed("localhost", "http://localhost", true, "same-origin", 80, 80));
    EXPECT_TRUE(localServerRequestAllowed("localhost:80", "http://localhost:80", true, "same-origin", 80, 80));
}
