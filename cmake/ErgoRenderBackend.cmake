# ---------------------------------------------------------------------------
# ErgoRenderBackend.cmake — ergo_render の実描画バックエンド契約を解決する。
#
# 責務は 1 つ: 「このビルド構成で ergo_render の実描画経路を有効化できるか」
# をプラットフォーム中立に判定し、 その結果 (有効可否 / プラットフォーム /
# Vulkan の出所 / リンクすべきライブラリ / 付与すべき定義) を返す。
#
# 判定の正本は **pictor ターゲットが公開する runtime Vulkan contract**
# (`PICTOR_HAS_VULKAN`) であり、 デスクトップ専用の import ターゲット
# `Vulkan::Vulkan` の有無ではない。 Android は NDK 同梱 libvulkan、 iOS は
# MoltenVK を pictor 側がリンクするため、 `find_package(Vulkan)` は失敗する。
# ここを desktop 前提で判定すると、 モバイル構成が「Vulkan が無い」と誤判定
# されて黙って Vulkan-free ビルドへ落ちる (KD-MOB-001)。
#
# 使い方:
#   include(ErgoRenderBackend)
#   ergo_render_resolve_backend(ERGO_RENDER_BACKEND)
#   # -> ERGO_RENDER_BACKEND_ENABLED / _PLATFORM / _SOURCE
#   #    / _LIBRARIES / _DEFINITIONS / _REASON
# ---------------------------------------------------------------------------

# pictor ターゲットが PICTOR_HAS_VULKAN を公開しているか。
# @implements spec/setup/build-cpp.md "ergo_render のクロスプラットフォーム契約"
# @implements spec/feature/module/render.md "(H) クロスプラットフォームビルド契約"
function(_ergo_render_pictor_has_vulkan out_var)
    set(${out_var} OFF PARENT_SCOPE)
    if(NOT TARGET pictor)
        return()
    endif()
    get_target_property(_defs pictor INTERFACE_COMPILE_DEFINITIONS)
    if(NOT _defs)
        return()
    endif()
    foreach(_def IN LISTS _defs)
        if(_def STREQUAL "PICTOR_HAS_VULKAN" OR _def STREQUAL "PICTOR_HAS_VULKAN=1")
            set(${out_var} ON PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

function(ergo_render_resolve_backend prefix)
    set(_enabled     OFF)
    set(_platform    "unknown")
    set(_source      "none")
    set(_libraries   "")
    set(_definitions "")
    set(_reason      "")

    # glslc は SPIR-V bake にだけ使う。 見つからなくても実描画可否の
    # 判定材料にはしない (モバイルはホスト側で bake 済みを配る)。
    find_package(Vulkan QUIET)

    _ergo_render_pictor_has_vulkan(_pictor_vulkan)

    if(NOT TARGET pictor)
        set(_reason "pictor target absent")
    elseif(ANDROID)
        set(_platform "android")
        if(_pictor_vulkan)
            # NDK 同梱 libvulkan は pictor が既にリンク済み。 ergo_render は
            # pictor 経由で Vulkan シンボルを得る (Vulkan::Vulkan は使わない)。
            set(_enabled ON)
            set(_source  "Android NDK")
            set(_definitions ERGO_RENDER_PLATFORM_ANDROID=1)
        else()
            set(_reason "pictor built without PICTOR_HAS_VULKAN (Android NDK Vulkan missing)")
        endif()
    elseif(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(_platform "ios")
        if(_pictor_vulkan)
            # iOS は MoltenVK。 実リンクはホスト xcodeproj 側 (または
            # -DVulkan_LIBRARIES 指定) で pictor が引き受ける。
            set(_enabled ON)
            set(_source  "MoltenVK")
            set(_definitions ERGO_RENDER_PLATFORM_IOS=1)
        else()
            set(_reason "pictor built without PICTOR_HAS_VULKAN (MoltenVK not wired)")
        endif()
    else()
        set(_platform "desktop")
        # Vulkan SDK が見つかっても、描画経路の正本である pictor の契約が
        # 無ければ ergo_render は有効化しない。 SDK だけで有効化すると、
        # Vulkan 無しで組まれた pictor のヘッダ/API に対して実描画コードを
        # コンパイルしてしまう。
        if(NOT _pictor_vulkan)
            set(_reason "pictor built without PICTOR_HAS_VULKAN")
        elseif(Vulkan_FOUND)
            set(_enabled ON)
            set(_source  "Vulkan SDK")
            set(_libraries Vulkan::Vulkan)
            set(_definitions ERGO_RENDER_PLATFORM_DESKTOP=1)
        else()
            # SDK の import ターゲットは無いが pictor が Vulkan を確保して
            # いる構成 (ホスト供給の SDK パスなど)。 pictor 経由で有効化する。
            set(_enabled ON)
            set(_source  "pictor-provided Vulkan")
            set(_definitions ERGO_RENDER_PLATFORM_DESKTOP=1)
        endif()
    endif()

    if(_enabled)
        list(APPEND _definitions ERGO_RENDER_HAS_VULKAN=1)
        # 出所名は空白を含む ("Vulkan SDK" 等)。 定義値の内側の `"` を
        # エスケープしないと、 コンパイラのコマンドラインでクォートが剥がれて
        # マクロが裸のトークン列 (`Vulkan SDK`) に展開され、
        # `return ERGO_RENDER_VULKAN_SOURCE;` が構文エラーになる。
        list(APPEND _definitions "ERGO_RENDER_VULKAN_SOURCE=\\\"${_source}\\\"")
    endif()

    set(${prefix}_ENABLED     ${_enabled}     PARENT_SCOPE)
    set(${prefix}_PLATFORM    "${_platform}"  PARENT_SCOPE)
    set(${prefix}_SOURCE      "${_source}"    PARENT_SCOPE)
    set(${prefix}_LIBRARIES   "${_libraries}" PARENT_SCOPE)
    set(${prefix}_DEFINITIONS "${_definitions}" PARENT_SCOPE)
    set(${prefix}_REASON      "${_reason}"    PARENT_SCOPE)
endfunction()

# 実描画が有効化できなかったときに、 その構成が「黙って Vulkan-free で成功」
# してよいかを判定して打ち切る。 モバイル (Android / iOS) は実描画以外の
# 用途を持たないため、 常に構成エラーにする。 デスクトップは Vulkan 非依存
# 部分のユニットテスト構成があるので既定では警告に留め、
# `ERGO_RENDER_REQUIRE_REAL=ON` で明示的な失敗へ格上げできる。
function(ergo_render_enforce_backend prefix)
    if(${prefix}_ENABLED)
        message(STATUS "ergo_render: real render path enabled (platform=${${prefix}_PLATFORM}, vulkan=${${prefix}_SOURCE})")
        return()
    endif()

    set(_msg "ergo_render: real render path unavailable (platform=${${prefix}_PLATFORM}, reason=${${prefix}_REASON})")
    if(ANDROID OR IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR ERGO_RENDER_REQUIRE_REAL)
        message(FATAL_ERROR "${_msg}")
    endif()
    message(WARNING "${_msg} — Vulkan-free build only")
endfunction()
