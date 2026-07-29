#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <iostream>
#include <future>
#include "MCDU/BezelLayout.h"
#include "Core/FMGC.h"
#include "Core/DataBus.h"
#include "MCDU/MCDU.h"

int main() {
    // Shared data bus (thread-safe, two unidirectional buses)
    DataBus bus;
    bus.setLatencyMs(30);  // simulate 30ms bus latency

    FMGC fmgc(bus);

    // Load navigation data in parallel (X-Plane format)
    auto fixFuture = std::async(std::launch::async, [&]() {
        return fmgc.loadXPlaneFix("ext_data/earth_fix.dat");
    });
    auto navFuture = std::async(std::launch::async, [&]() {
        return fmgc.loadXPlaneNav("ext_data/earth_nav.dat");
    });
    auto aptFuture = std::async(std::launch::async, [&]() {
        return fmgc.loadAirportsCSV("ext_data/airports.csv");
    });

    MCDU mcdu(fmgc, bus);
    mcdu.rebuildScreen();

    int fixCount = fixFuture.get();
    int navCount = navFuture.get();
    int aptCount = aptFuture.get();
    std::cout << "NavDatabase: " << fixCount << " fixes, "
              << navCount << " navaids, " << aptCount << " airports loaded\n";

    fmgc.start();  // 20 Hz FMGC thread

    sf::RenderWindow window(
        sf::VideoMode({static_cast<unsigned int>(BEZEL_W),
                       static_cast<unsigned int>(BEZEL_H)}),
        "A320 MCDU",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {BEZEL_W, BEZEL_H})));

    while (window.isOpen()) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            // Mouse click -> LSK hit test
            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    MCDUButton btn = mcdu.hitTestLsk(
                        static_cast<float>(mb->position.x),
                        static_cast<float>(mb->position.y));
                    if (btn != MCDU::NO_HIT)
                        mcdu.handleButton(btn);
                }
            }

            // Keyboard -> scratchpad (auto-uppercase)
            if (const auto* txt = event->getIf<sf::Event::TextEntered>()) {
                mcdu.inputChar(static_cast<char>(txt->unicode));
            }

            // Special keys
            if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                using Key = sf::Keyboard::Key;
                if (kp->code == Key::Backspace)
                    mcdu.handleButton(MCDUButton::CLR);
                if (kp->code == Key::Escape)
                    window.close();
            }
        }

        // Tick bus and rebuild screen (FMGC thread handles processMessages)
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        mcdu.updateBus(now);        // tick bus — latches pending messages
        mcdu.rebuildScreen();       // polls FMGC responses, builds screen

        window.clear(sf::Color::Black);
        mcdu.render(window);
        window.display();
    }

    fmgc.stop();
    return 0;
}
