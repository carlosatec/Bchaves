#pragma once

#include "system/types.hpp"

namespace qchaves::engine {

int run_address(const qchaves::system::AddressOptions& options);
int run_bsgs(const qchaves::system::BsgsOptions& options);
int run_kangaroo(const qchaves::system::KangarooOptions& options);

}  // namespace qchaves::engine
