/*
 * Atomic operations wrapper.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_ATOMIC_H
#define LIBROCE_ATOMIC_H

#define roce_atomic_inc(ptr) __atomic_fetch_add(ptr, 1, __ATOMIC_SEQ_CST)
#define roce_atomic_dec(ptr) __atomic_fetch_sub(ptr, 1, __ATOMIC_SEQ_CST)
#define roce_atomic_read(ptr) __atomic_load_n(ptr, __ATOMIC_RELAXED)

#endif /* LIBROCE_ATOMIC_H */
