new_http_archive(
    name = "gmock_archive",
    url = "https://googlemock.googlecode.com/files/gmock-1.7.0.zip",
    sha256 = "26fcbb5925b74ad5fc8c26b0495dfc96353f4d553492eb97e85a8a6d2f43095b",
    build_file = "gmock.BUILD",
)

bind(
    name = "gtest",
    actual = "@gmock_archive//:gtest",
)

bind(
    name = "gtest_main",
    actual = "@gmock_archive//:gtest_main",
)

new_http_archive(
    name = "cpython",
    build_file = "cpython.BUILD",
    url = "https://www.python.org/ftp/python/2.7.12/Python-2.7.12.tgz",
    sha256 = "3cb522d17463dfa69a155ab18cffa399b358c966c0363d6c8b5b3bf1384da4b6",
    strip_prefix = "Python-2.7.12",
)
