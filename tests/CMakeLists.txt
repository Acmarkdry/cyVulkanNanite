# tests/CMakeLists.txt
# Nanite 核心模块测试 — 不依赖 Vulkan 运行时

include(FetchContent)

# ── Google Test ──────────────────────────────────────────────
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# Windows: 强制使用与主项目一致的运行时库
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── RapidCheck ───────────────────────────────────────────────
FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG        ff6571e  # 锁定到稳定 commit，避免 master 随时 break
)
set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rapidcheck)

# ── 测试源文件（自动收集，新增文件无需手动修改此处）────────
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

# ── 测试可执行目标 ───────────────────────────────────────────
add_executable(nanite_tests ${TEST_SOURCES})

# nanite_core 的 PUBLIC include 路径会自动传递，无需手动重复
target_link_libraries(nanite_tests PRIVATE
    nanite_core
    GTest::gtest_main
    rapidcheck
    rapidcheck_gtest
)

# ── CTest 集成 ───────────────────────────────────────────────
include(GoogleTest)
gtest_discover_tests(nanite_tests)
