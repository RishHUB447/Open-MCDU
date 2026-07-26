#pragma once
#include <iostream>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <functional>

enum class MCDUButton {
    L1, L2, L3, L4, L5, L6,
    R1, R2, R3, R4, R5, R6,
    CLR, DEL, ENT, MENU, FPLN, INIT_REF,
    PREV_PAGE, NEXT_PAGE
};

struct button {
    sf::Vector2f position;
    sf::Vector2f size;
    std::function<void()> on_click;
};

class MCDU {

};