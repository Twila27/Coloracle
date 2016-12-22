#include "Engine/Math/Interval.hpp"
#include "Engine/EngineCommon.hpp"

template <>
STATIC const Interval<float> Interval<float>::ZERO = Interval( 0.f );
STATIC const Interval<int> Interval<int>::ZERO = Interval( 0 );
STATIC const Interval<double> Interval<double>::ZERO = Interval( 0.0 );