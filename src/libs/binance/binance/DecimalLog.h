#pragma once

#include "Decimal.h"

// Note: this seems to be the actual requirement to compile.
#include <spdlog/fmt/ostr.h> // must be included

// see: https://fmt.dev/latest/api.html#format-api
template <>
struct fmt::formatter<ad::Decimal>
{
  // Parses nothing
  constexpr auto parse(format_parse_context& ctx)
  {
    // Parse the presentation format and store it in the formatter:
    auto it = ctx.begin(), end = ctx.end();

    // Check if reached the end of the range:
    if (it != end && *it != '}')
      throw format_error("invalid format");

    // Return an iterator past the end of the parsed range:
    return it;
  }

  // Formats the Decimal to its exact precision
  template <typename FormatContext>
  auto format(const ad::Decimal & aDecimal, FormatContext& ctx)
  {
    std::ostringstream os;
    auto previous = os.precision(std::numeric_limits<ad::Decimal>::digits10);
    os << std::showpoint << std::fixed << aDecimal;
    os.precision(previous);

    // ctx.out() is an output iterator to write to.
    return format_to(
        ctx.out(),
        "{}",
        os.str()
    );
  }
};
