#!/bin/sh
# Build and run the core tests (no JUCE needed). If node is installed, also compares the C++ core with the mockup's JS model.
set -e
cd "$(dirname "$0")/.."
T="${TMPDIR:-/tmp}"
g++ -std=c++17 -O2 -Wall -Wextra -o "$T/lap_test" tests/test_core.cpp
"$T/lap_test"
if command -v node >/dev/null 2>&1 && [ -f docs/mockup/LapCat2A.html ]; then
  g++ -std=c++17 -O2 -Wall -Wextra -o "$T/lap_parity" tests/parity_cpp.cpp
  node tests/parity.js "$T/lap_parity"
fi

# Studio 87 Gear Updater link (product id and link string)
(cd "$(dirname "$0")/.." && g++ -std=c++17 -O2 -Wall -Wextra -o /tmp/s87_link_test tests/test_updater_link.cpp && /tmp/s87_link_test) || exit 1
