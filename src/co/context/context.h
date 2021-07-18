/*!The Treasure Box Library
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Copyright (C) 2009 - 2018, TBOOX Open Source Group.
 *
 * @author      ruki
 * @file        context.h
 * @ingroup     platform
 *
 * modified by Alvin at 2018/12/21, 2021/04/09
 */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * context for a coroutine 
 *   - It always points to the stack bottom, see more details on: 
 *     https://tboox.org/cn/2016/10/28/coroutine-context/
 */
typedef void* tb_context_t;

/*! the context-from type
 *
 * @ctx    the from-context
 * @priv   the passed user private data
 */
typedef struct __tb_context_from_t {
    tb_context_t ctx;
    const void* priv;
} tb_context_from_t;

/*! the context entry function type
 *
 * @param from          the from-context
 */
typedef void (*tb_context_func_t)(tb_context_from_t from);

/*! make context from the given stack space and the callback function
 *
 * @param stackdata     the stack data
 * @param stacksize     the stack size
 * @param func          the entry function
 *
 * @return              the context pointer
 */
tb_context_t tb_context_make(char* stackdata, size_t stacksize, tb_context_func_t func);

/*! jump to the given context
 *
 * @param ctx           the to-context
 * @param priv          the passed user private data
 *
 * @return              the from-context
 */
tb_context_from_t tb_context_jump(tb_context_t ctx, const void* priv);

#ifdef __cplusplus
}
#endif
