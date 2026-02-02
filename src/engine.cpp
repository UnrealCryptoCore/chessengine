#include "uci.h"
#include "game.h"
#include <cstddef>
#include <cstdlib>
#include <ctime>

int main() {
    std::srand(time(NULL));
    Mondfisch::initConstants();

    Mondfisch::UciEngine engine{};
    engine.loop();
}
