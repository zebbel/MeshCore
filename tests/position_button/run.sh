#!/usr/bin/env bash
set -euo pipefail
repo="$(cd -- "$(dirname -- "$0")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf -- "$tmp"' EXIT
# Copy the unmodified implementation so its quoted MyMesh include resolves to
# the host hardware/filesystem stand-ins; do not compile the Arduino runtime.
cp "$repo/examples/companion_radio/PositionButton.cpp" "$tmp/PositionButton.cpp"
cp "$repo/examples/companion_radio/CustomCLI.cpp" "$tmp/CustomCLI.cpp"
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined \
  -I "$repo" "$repo/tests/position_button/test_state.cpp" -o "$tmp/state"
"$tmp/state"
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined \
  -I "$repo" "$repo/tests/position_button/test_local_help.cpp" -o "$tmp/local_help"
"$tmp/local_help"
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -DENABLE_POSITION_BUTTON \
  -I "$repo/tests/position_button/stubs" -I "$repo/examples/companion_radio" \
  "$tmp/PositionButton.cpp" "$repo/tests/position_button/test_integration.cpp" -o "$tmp/integration"
"$tmp/integration"
echo 'Position state and firmware integration tests passed.'

g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -DENABLE_POSITION_BUTTON \
  -I "$repo/tests/position_button/stubs" -I "$repo/examples/companion_radio" \
  "$tmp/PositionButton.cpp" "$tmp/CustomCLI.cpp" "$repo/tests/position_button/test_cli.cpp" -o "$tmp/cli"
"$tmp/cli"
