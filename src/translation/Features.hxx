/*
 * Copyright 2006-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This header enables or disables certain features of the translation
 * client.  More specifically, it can be used to eliminate
 * #TranslateRequest and #TranslateResponse attributes.
 */

#ifndef BENG_PROXY_TRANSLATION_FEATURES_HXX
#define BENG_PROXY_TRANSLATION_FEATURES_HXX

#define TRANSLATION_ENABLE_CACHE 0
#define TRANSLATION_ENABLE_WANT 0
#define TRANSLATION_ENABLE_EXPAND 0
#define TRANSLATION_ENABLE_SESSION 0
#define TRANSLATION_ENABLE_HTTP 0
#define TRANSLATION_ENABLE_WIDGET 0
#define TRANSLATION_ENABLE_RADDRESS 0
#define TRANSLATION_ENABLE_TRANSFORMATION 0
#define TRANSLATION_ENABLE_EXECUTE 1
#define TRANSLATION_ENABLE_JAILCGI 0

#endif
