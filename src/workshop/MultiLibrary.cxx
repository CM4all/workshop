/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "MultiLibrary.hxx"

std::shared_ptr<Plan>
MultiLibrary::Get(const char *name)
{
    for (auto &i : libraries) {
        auto p = i.Get(name);
        if (p)
            return p;
    }

    return nullptr;
}
