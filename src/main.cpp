#include "Core/BezelLayout.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <array>
#include <string>

int main() {
    sf::Texture bg_tex;
    bool bg_ok = bg_tex.loadFromFile("assets/MCDU.png");
    if (!bg_ok)
        std::cerr << "WARN: Could not load assets/MCDU.png";

    sf::Font font;
    bool font_ok = font.openFromFile("assets/B612-Regular.ttf");
    if (!font_ok)
        std::cerr << "WARN: Could not load assets/B612-Regular.ttf";

    sf::RenderWindow window(
        sf::VideoMode({static_cast<unsigned int>(BEZEL_W),
                       static_cast<unsigned int>(BEZEL_H)}),
        "A320 MCDU - Grid Test",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {BEZEL_W, BEZEL_H})));

    sf::Sprite bg_sprite(bg_tex);

    std::array<std::array<char, LCD_COLS>, LCD_ROWS> grid{};
    for (int r = 0; r < LCD_ROWS; r++) {
        for (int c = 0; c < LCD_COLS; c++) {
            switch (r) {
                case 0:  grid[r][c] = 'A' + (c % 26); break;
                case 1:  grid[r][c] = '0' + (c % 10); break;
                case 2:  grid[r][c] = '#';            break;
                case 3:  grid[r][c] = '.';            break;
                case 4:  grid[r][c] = '+';            break;
                case 5:  grid[r][c] = '-';            break;
                case 6:  grid[r][c] = '/';            break;
                case 7:  grid[r][c] = '=';            break;
                default: grid[r][c] = grid[r % 8][c]; break;
            }
        }
    }

    sf::RectangleShape lcd_bg({LCD_W, LCD_H});
    lcd_bg.setPosition({LCD_X, LCD_Y});
    lcd_bg.setFillColor(sf::Color(0, 0, 0, 180));

    sf::RectangleShape grid_line;
    grid_line.setFillColor(sf::Color(0, 180, 0, 80));

    sf::RectangleShape lsk_rect({LSK_W, LSK_H});
    lsk_rect.setFillColor(sf::Color(255, 255, 0, 30));
    lsk_rect.setOutlineColor(sf::Color(255, 255, 0, 180));
    lsk_rect.setOutlineThickness(1.f);

    sf::RectangleShape rsk_rect({LSK_W, LSK_H});
    rsk_rect.setFillColor(sf::Color(255, 255, 0, 30));
    rsk_rect.setOutlineColor(sf::Color(255, 255, 0, 180));
    rsk_rect.setOutlineThickness(1.f);

    while (window.isOpen()) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        window.clear(sf::Color::Black);

        if (bg_ok) {
            bg_sprite.setPosition({0, 0});
            window.draw(bg_sprite);
        } else {
            sf::RectangleShape bezel_placeholder({BEZEL_W, BEZEL_H});
            bezel_placeholder.setFillColor(sf::Color(30, 30, 30));
            bezel_placeholder.setOutlineColor(sf::Color(100, 100, 100));
            bezel_placeholder.setOutlineThickness(2.f);
            window.draw(bezel_placeholder);
        }

        window.draw(lcd_bg);

        // Vertical grid lines
        for (int c = 0; c <= LCD_COLS; c++) {
            float x = LCD_X + c * CELL_W;
            grid_line.setSize({1.f, LCD_H});
            grid_line.setPosition({x, LCD_Y});
            window.draw(grid_line);
        }

        // Horizontal grid lines
        for (int r = 0; r <= LCD_ROWS; r++) {
            float y = LCD_Y + r * CELL_H;
            grid_line.setSize({LCD_W, 1.f});
            grid_line.setPosition({LCD_X, y});
            window.draw(grid_line);
        }

        // Chars test
        if (font_ok) {
            sf::Text cell_text(font, "", 14);
            cell_text.setFillColor(sf::Color(0, 230, 0));
            for (int r = 0; r < LCD_ROWS; r++) {
                for (int c = 0; c < LCD_COLS; c++) {
                    char ch[2] = {grid[r][c], '\0'};
                    cell_text.setString(ch);
                    sf::FloatRect tb = cell_text.getLocalBounds();
                    float px = LCD_X + c * CELL_W + (CELL_W - tb.size.x) / 2.f;
                    float py = LCD_Y + r * CELL_H + (CELL_H - tb.size.y) / 2.f - tb.position.y;
                    cell_text.setPosition({px, py});
                    window.draw(cell_text);
                }
            }
        } else {
            sf::RectangleShape cell_fill({CELL_W - 1, CELL_H - 1});
            cell_fill.setFillColor(sf::Color(0, 80, 0, 100));
            for (int r = 0; r < LCD_ROWS; r++) {
                for (int c = 0; c < LCD_COLS; c++) {
                    cell_fill.setPosition({LCD_X + c * CELL_W + 0.5f, LCD_Y + r * CELL_H + 0.5f});
                    window.draw(cell_fill);
                }
            }
        }

        // LSK button overlays
        for (int i = 0; i < 6; i++) {
            float y = LSK1_Y + i * LSK_PITCH;
            lsk_rect.setPosition({LSK_X, y});
            window.draw(lsk_rect);
            rsk_rect.setPosition({RSK_X, y});
            window.draw(rsk_rect);
        }

        // Info
        if (font_ok) {
            sf::Text info(font, "14R x 24C  Cell: " +
                std::to_string(CELL_W).substr(0, 5) + "x" +
                std::to_string(CELL_H).substr(0, 5) + "px  " +
                (bg_ok ? "" : "[NO BEZEL IMG] ") +
                (font_ok ? "" : "[NO FONT]"),
                12);
            info.setFillColor(sf::Color::White);
            info.setPosition({5.f, 5.f});
            window.draw(info);
        }

        window.display();
    }
    return 0;
}
