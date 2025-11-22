#include "uiManager.hpp"

void UIManager::cleanup() {
    for (auto& element : elements) {
        delete element;
    }
    elements.clear();

    this->muim.cleanup();
}

void UIManager::addElement(UIElement* element) {
    this->elements.push_back(element);
}

void UIManager::update() {
    for (auto& element : elements) {
        element->update();
    }
    this->muim.update();
}

void UIManager::draw() {
    for (auto& element : elements) {
        element->draw();
    }
    this->muim.draw();
}

Texture2D UIManager::loadTexture(const std::string& fileName) {
    return LoadTexture(fileName.c_str());
}