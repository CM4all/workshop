/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef WORKSHOP_PLAN_LOADER_HXX
#define WORKSHOP_PLAN_LOADER_HXX

#include <boost/filesystem.hpp>

struct Plan;

/**
 * Parses plan configuration files.
 */
Plan
LoadPlanFile(const boost::filesystem::path &path);

#endif
