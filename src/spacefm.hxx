/*
 *
 * Copyright: See the LICENSE file that came with this program
 *
 */

#pragma once

namespace SpaceFM
{
    template<typename T> using Ref = std::shared_ptr<T>;
    template<typename T, typename... Args> constexpr Ref<T> CreateRef(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

} // namespace SpaceFM
