// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class EmailService;
struct CronJob;
struct CronResult;

void
SendNotificationEmail(EmailService &service, const CronJob &job,
		      const CronResult &result) noexcept;
