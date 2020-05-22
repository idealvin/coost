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
 * @file        prefix.S
 *
 * modified by Alvin at 2018/12/21
 */

/* //////////////////////////////////////////////////////////////////////////////////////
 * macros
 */

/*! function
 * 
 * @code
    function func_xxxx, export=1
        ...
    endfunc
   @endcode
 */
.macro function name, export=0
    .macro endfunc
        .end \name
        .size \name, . - \name
        .purgem endfunc
    .endm

        .text
        .align CPU_BYTE_SIZE
    .if \export
        .global \name
        .type   \name, @function
        .hidden \name
        .ent \name
\name:
    .else
        .type   \name, @function
        .hidden \name
        .ent \name
\name:
    .endif
.endm


