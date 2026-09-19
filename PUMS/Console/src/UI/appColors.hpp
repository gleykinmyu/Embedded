#pragma once

#include "nex.hpp"

namespace server {

struct AppColors {
    static constexpr nex::Color kPage{4258u};
    static constexpr nex::Color kDefault{10565u};
    static constexpr nex::Color kMain{64800u};
    /** GRUP / шоуфайл. */
    static constexpr nex::Color kGroupBlocked{20643u};
    /** Сегментный Block на сервере (приоритетнее GRUP). */
    static constexpr nex::Color kServerBlocked{57504u};
    static constexpr nex::Color kBorder{21130u};

    static constexpr nex::Color kText{61277u};
    static constexpr nex::Color kTextLight{65535u};
};

} // namespace server
