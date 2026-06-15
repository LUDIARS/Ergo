#pragma once

/// ergo::math — umbrella header.
///
/// Including this header pulls in all ergo_math sub-headers:
///   concepts  — Scalar / Arithmetic C++20 concepts
///   vec       — generic Vec<N,T> template + type aliases
///   mat       — generic Mat<R,C,T> template + Mat2/3/4 helpers
///   batch     — SoA batch operations (add/scale/madd/dot)
///   pool      — Arena bump allocator + ObjectPool<T>
///
/// For ergo::vector interop also include ergo/math/vec_vector_convert.h
/// after ergo/vector/vector.h.
///
/// Spec: spec/module/math.md

#include "ergo/math/concepts.h"
#include "ergo/math/vec.h"
#include "ergo/math/mat.h"
#include "ergo/math/batch.h"
#include "ergo/math/pool.h"
