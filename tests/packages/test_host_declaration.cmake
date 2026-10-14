# W1 (spec/feature/package-manager.md §2.1 / §7): the manifest reader and a
# host declaration (ergo-packages.json with a `host` section).
#   * read as the Ergo manifest -> FATAL_ERROR (a configure error)
#   * read with HOST            -> host code and private packages
#   * a private name equal to an Ergo manifest name -> PKG-15 error
#   * a legacy extra manifest without `host` still reads with HOST
# No compiler, no git, no network.
include("${CMAKE_CURRENT_LIST_DIR}/pkg_test_support.cmake")

file(TO_CMAKE_PATH "${ERGO_TEST_WORK_DIR}" W)
file(REMOVE_RECURSE "${W}")
file(MAKE_DIRECTORY "${W}")

set(SHA_A 1111111111111111111111111111111111111111)
set(SHA_B 2222222222222222222222222222222222222222)

file(WRITE "${W}/ergo.json" "{
  \"schema\": 1,
  \"packages\": {
    \"pictor\": { \"kind\": \"link\", \"code\": \"Pc\", \"repository\": \"https://github.com/LUDIARS/Pictor.git\", \"rev\": \"${SHA_A}\" }
  },
  \"modules\": { \"ergo_render\": { \"needs\": [\"pictor\"] } },
  \"observe\": {}
}
")
file(WRITE "${W}/host-private.json" "{
  \"schema\": 1,
  \"host\": {
    \"code\": \"Example\",
    \"ergo\": { \"repository\": \"https://github.com/LUDIARS/Ergo.git\", \"rev\": \"${SHA_A}\" },
    \"use\": [\"pictor\", \"privlib\"]
  },
  \"packages\": {
    \"privlib\": { \"kind\": \"link\", \"code\": \"Example\", \"repository\": \"https://example.invalid/Privlib.git\", \"rev\": \"${SHA_B}\" }
  }
}
")
file(WRITE "${W}/host-clash.json" "{
  \"schema\": 1,
  \"host\": { \"code\": \"Example\" },
  \"packages\": {
    \"pictor\": { \"kind\": \"link\", \"repository\": \"https://example.invalid/Pictor.git\", \"rev\": \"${SHA_B}\" }
  }
}
")
file(WRITE "${W}/legacy-extra.json" "{
  \"schema\": 1,
  \"packages\": {
    \"privlib\": { \"kind\": \"link\", \"repository\": \"https://example.invalid/Privlib.git\", \"rev\": \"${SHA_B}\" }
  }
}
")

# --- a host declaration is not an Ergo manifest (configure error) ------------
file(WRITE "${W}/load_as_ergo.cmake"
    "include(\"${ERGO_SOURCE_DIR}/cmake/packages/ErgoPackageManifest.cmake\")\n"
    "ergo_pkg_manifest_load(_m \"${W}/host-private.json\")\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -P "${W}/load_as_ergo.cmake"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(_rc EQUAL 0)
    pkg_fail("a host declaration read as the Ergo manifest must stop")
endif()
if(NOT "${_out}${_err}" MATCHES "host[ \n]+declaration,[ \n]+not[ \n]+an[ \n]+Ergo[ \n]+manifest")
    pkg_fail("host-as-manifest error lacks the reason:\n${_out}${_err}")
endif()
pkg_pass("host declaration rejected as an Ergo manifest")

# --- HOST mode: code and private packages -----------------------------------
ergo_pkg_manifest_load(h "${W}/host-private.json" HOST)
pkg_assert_eq("host present" "${h_HOST_PRESENT}" "TRUE")
pkg_assert_eq("host code" "${h_HOST_CODE}" "Example")
pkg_assert_eq("private packages" "${h_PACKAGES}" "privlib")
pkg_assert_eq("private link" "${h_LINK}" "privlib")
pkg_assert_eq("private rev" "${h_PKG_privlib_REV}" "${SHA_B}")

ergo_pkg_manifest_load(e "${W}/ergo.json")
pkg_assert_eq("ergo manifest has no host" "${e_HOST_PRESENT}" "FALSE")
ergo_pkg_manifest_merge(e h f)
pkg_assert_empty("private name is new" "${f}")
pkg_assert_eq("merged link" "${e_LINK}" "pictor;privlib")
pkg_pass("host declaration (private packages) merged")

# --- PKG-15: private name reuses an Ergo manifest name -------------------------
ergo_pkg_manifest_load(e2 "${W}/ergo.json")
ergo_pkg_manifest_load(c "${W}/host-clash.json" HOST)
ergo_pkg_manifest_merge(e2 c f)
pkg_assert_finding("private clash" "${f}" PKG-15 error pictor)
pkg_assert_eq("public pin kept" "${e2_PKG_pictor_REV}" "${SHA_A}")
pkg_pass("PKG-15 on a private name equal to an Ergo manifest name")

# --- legacy extra manifest (no host section) ---------------------------------
ergo_pkg_manifest_load(l "${W}/legacy-extra.json" HOST)
pkg_assert_eq("legacy has no host" "${l_HOST_PRESENT}" "FALSE")
pkg_assert_eq("legacy code empty" "${l_HOST_CODE}" "")
pkg_assert_eq("legacy packages" "${l_PACKAGES}" "privlib")
pkg_pass("legacy extra manifest still reads")
