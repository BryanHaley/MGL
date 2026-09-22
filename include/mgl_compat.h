/*
 * Copyright (C) The MooGL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * mgl_compat.h
 * MGL
 *
 */

#ifndef mgl_compat_h
#define mgl_compat_h

/*
 * Bits of the compatibility profile that shipped games still reach for.
 *
 * MGL is a core profile driver, but old engines and old ports keep using a
 * handful of removed features, and each one is cheap next to what it buys.
 * They are on by default. Build with -DMGL_NO_COMPAT_PROFILE for a driver
 * that answers only for core, which is what the conformance suite expects.
 */

#if !defined(MGL_NO_COMPAT_PROFILE) && !defined(MGL_COMPAT_PROFILE)
#define MGL_COMPAT_PROFILE 1
#endif


#ifdef MGL_COMPAT_PROFILE
/* Enums the core headers dropped, which MGL still has to recognise. */
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST      0x0BC0
#endif
#ifndef GL_ALPHA_TEST_FUNC
#define GL_ALPHA_TEST_FUNC 0x0BC1
#endif
#ifndef GL_ALPHA_TEST_REF
#define GL_ALPHA_TEST_REF  0x0BC2
#endif
/* The old single clamp, which clamped to the border colour. */
#ifndef GL_CLAMP
#define GL_CLAMP           0x2900
#endif
#endif /* MGL_COMPAT_PROFILE */

#endif /* mgl_compat_h */
