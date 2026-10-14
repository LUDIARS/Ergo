# The real ergo-packages.json: schema, pins (PKG-02), declared graph (PKG-12),
# provided targets (PKG-15) and the reader's handling of fixture manifests.
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

ergo_pkg_manifest_load(m "${ERGO_SOURCE_DIR}/ergo-packages.json")
pkg_assert_eq("schema" "${m_SCHEMA}" "1")
pkg_assert_empty("schema errors" "${m_ERRORS}")
pkg_assert_eq("link packages" "${m_LINK}" "pictor;tela")
pkg_assert_eq("tool packages" "${m_TOOLS}" "lapilli")
pkg_assert_eq("modules" "${m_MODULES}"
    "ergo_particle;ergo_render;ergo_render_viz;ergo_resource;ergo_tela_graph")
pkg_assert_eq("observe" "${m_OBSERVE}" "ars-musa;clio;curare;terpsichore;thaleia")

# Every link pin is a full SHA.
foreach(n IN LISTS m_LINK)
    ergo_pkg_rule_rev_format(f PACKAGE ${n} REV "${m_PKG_${n}_REV}")
    pkg_assert_empty("PKG-02 manifest pin ${n}" "${f}")
endforeach()

# Pictor details used by the rules.
pkg_assert_eq("pictor targets" "${m_PKG_pictor_TARGETS}" "pictor")
pkg_assert_eq("pictor features" "${m_PKG_pictor_FEATURES}" "d3d12;metal;vulkan;web;webgpu")
pkg_assert_eq("pictor vulkan platforms" "${m_PKG_pictor_FEAT_vulkan_PLATFORMS}" "desktop;android;ios")
pkg_assert_eq("pictor vulkan contract" "${m_PKG_pictor_FEAT_vulkan_CONTRACT}" "PICTOR_HAS_VULKAN")
pkg_assert_eq("pictor webgpu reserved" "${m_PKG_pictor_FEAT_webgpu_RESERVED}" "TRUE")
pkg_assert_eq("pictor vulkan not reserved" "${m_PKG_pictor_FEAT_vulkan_RESERVED}" "FALSE")
pkg_assert_eq("pictor cache" "${m_PKG_pictor_CACHE}"
    "PICTOR_BUILD_DEMO=OFF;PICTOR_BUILD_TESTS=OFF;PICTOR_BUILD_TOOLS=OFF")
pkg_assert_eq("tela targets" "${m_PKG_tela_TARGETS}" "Tela::Core;Tela::Graph")
pkg_assert_eq("tela find_package" "${m_PKG_tela_FIND_PACKAGE}" "Tela")
pkg_assert_eq("lapilli path" "${m_PKG_lapilli_PATH}" "lib/lapilli")
pkg_assert_eq("render needs" "${m_MOD_ergo_render_NEEDS}" "pictor")
pkg_assert_eq("render need kind" "${m_MOD_ergo_render_NEED_KIND}" "real-path")
pkg_assert_eq("resource ported_from" "${m_MOD_ergo_resource_PORTED_FROM}" "clio")
pkg_assert_eq("curare relations" "${m_OBS_curare_RELATIONS}" "ergo_resource:shares-vocabulary")

# Unselected default features are valid on every declared platform.
foreach(platform desktop android ios emscripten)
    set(defaults "")
    set(platforms "")
    set(reserved "")
    foreach(ft IN LISTS m_PKG_pictor_FEATURES)
        list(APPEND defaults "${ft}=${m_PKG_pictor_FEAT_${ft}_DEFAULT}")
        string(REPLACE ";" "," pl "${m_PKG_pictor_FEAT_${ft}_PLATFORMS}")
        list(APPEND platforms "${ft}=${pl}")
        if(m_PKG_pictor_FEAT_${ft}_RESERVED)
            list(APPEND reserved ${ft})
        endif()
    endforeach()
    ergo_pkg_features_select(sel f PACKAGE pictor PLATFORM ${platform}
        FEATURES ${m_PKG_pictor_FEATURES} DEFAULTS ${defaults} PLATFORMS ${platforms}
        RESERVED ${reserved})
    pkg_assert_empty("pictor defaults on ${platform}" "${f}")
    ergo_pkg_rule_feature_combination(f PACKAGE pictor PLATFORM ${platform}
        SELECTED ${sel} PLATFORMS ${platforms} RESERVED ${reserved}
        REQUIRE_ANY vulkan web)
    pkg_assert_empty("pictor default combination on ${platform}" "${f}")
endforeach()

# Declared graph has no cycle, no target is claimed twice.
set(edges "")
set(provided "")
foreach(n IN LISTS m_LINK)
    foreach(r IN LISTS m_PKG_${n}_REQUIRES)
        list(APPEND edges "${n}>${r}")
    endforeach()
    foreach(t IN LISTS m_PKG_${n}_TARGETS)
        list(APPEND provided "${n}:${t}")
    endforeach()
endforeach()
ergo_pkg_rule_cycles(f NODES ${m_LINK} EDGES ${edges})
pkg_assert_empty("PKG-12 manifest graph" "${f}")
ergo_pkg_rule_collisions(f PROVIDED ${provided})
pkg_assert_empty("PKG-15 manifest targets" "${f}")
pkg_pass("real manifest")

# Fixture manifests: schema mismatch and bad kind are reported, not fatal.
file(MAKE_DIRECTORY "${ERGO_TEST_WORK_DIR}")
file(WRITE "${ERGO_TEST_WORK_DIR}/bad.json"
    "{ \"schema\": 2, \"packages\": { \"x\": { \"kind\": \"weird\" } } }")
ergo_pkg_manifest_load(b "${ERGO_TEST_WORK_DIR}/bad.json")
list(LENGTH b_ERRORS n_errors)
pkg_assert_eq("fixture error count" "${n_errors}" "2")
pkg_assert_eq("fixture no link" "${b_LINK}" "")
pkg_assert_eq("fixture absent sections" "${b_MODULES}${b_OBSERVE}" "")
pkg_pass("fixture manifest")
