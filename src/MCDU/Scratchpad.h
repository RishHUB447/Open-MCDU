#pragma once
#include <string>

class Scratchpad {
public:
    bool isMessage() const { return m_isMessage; }
    bool isEmpty() const { return m_text.empty(); }
    const std::string& text() const { return m_text; }

    void inputChar(char c) {
        if (m_isMessage)
            clear();
        m_text.push_back(c);
    }

    void deleteChar() {
        if (!m_text.empty() && !m_isMessage)
            m_text.pop_back();
    }

    void clear() {
        m_text.clear();
        m_isMessage = false;
    }

    void showMessage(const std::string& msg) {
        m_text = msg;
        m_isMessage = true;
    }

    void setText(const std::string& t) {
        m_text = t;
        m_isMessage = false;
    }

    std::string consume() {
        std::string result = m_text;
        clear();
        return result;
    }

private:
    std::string m_text;
    bool m_isMessage = false;
};
