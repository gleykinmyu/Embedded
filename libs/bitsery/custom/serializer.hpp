/**
 * @file bitsery/custom/serializer.hpp
 * @brief Reader, Writer и Measure поверх MSB-first адаптеров.
 *
 * Псевдонимы штатных Serializer и Deserializer. Своего класса сериализатора нет.
 */

#pragma once

#include <bitsery/bitsery.h>

#include "msb_bitpack.hpp"

namespace bitsery::custom {

using Writer = bitsery::Serializer<msb::OutputAdapter<>>;
using Reader = bitsery::Deserializer<msb::InputAdapter<>>;
using Measure = bitsery::Serializer<msb::MeasureAdapter<>>;

} // namespace bitsery::custom
