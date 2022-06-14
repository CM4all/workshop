/*
 * Copyright 2006-2022 CM4all GmbH
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

#include "SpawnClient.hxx"
#include "Marshal.hxx"
#include "Send.hxx"
#include "Receive.hxx"
#include "translation/Protocol.hxx"
#include "translation/Response.hxx"
#include "AllocatorPtr.hxx"
#include "net/SocketDescriptor.hxx"

#include <stdexcept>

using std::string_view_literals::operator""sv;

static void
SendTranslateSpawn(SocketDescriptor s, const char *tag,
		   const char *plan_name,
		   const char *execute, const char *param)

{
	assert(execute != nullptr);

	if (strlen(execute) > 1024)
		throw std::runtime_error("Translation parameter too long");

	if (param != nullptr && strlen(param) > 4096)
		throw std::runtime_error("Translation parameter too long");

	TranslationMarshaller m;

	m.Write(TranslationCommand::BEGIN);

	m.Write(TranslationCommand::EXECUTE, execute);
	m.Write(TranslationCommand::PLAN, plan_name);
	m.Write(TranslationCommand::SERVICE, "workshop"sv);

	if (param != nullptr)
		m.Write(TranslationCommand::PARAM, param);

	if (tag != nullptr)
		m.Write(TranslationCommand::LISTENER_TAG, tag);

	m.Write(TranslationCommand::END);

	SendFull(s, m.Commit());
}

TranslateResponse
TranslateSpawn(AllocatorPtr alloc, SocketDescriptor s, const char *tag,
	       const char *plan_name, const char *execute, const char *param)
{
	SendTranslateSpawn(s, tag, plan_name, execute, param);
	return ReceiveTranslateResponse(alloc, s);
}
