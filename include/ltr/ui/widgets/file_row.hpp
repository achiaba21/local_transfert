#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Window/Event.hpp>

namespace ltr::ui {

class FileRow {
public:
    // V1.1.5 : une FileRow peut représenter un fichier OU un dossier groupé.
    enum class Kind { File, Folder };

    FileRow& setBounds(const sf::FloatRect& r) { bounds_ = r; return *this; }
    FileRow& setName(const std::string& s)     { name_   = s; return *this; }
    FileRow& setSize(std::uint64_t n)          { size_   = n; return *this; }
    FileRow& onRemove(std::function<void()> cb){ cb_     = std::move(cb); return *this; }

    // V1.1 : case à cocher à gauche.
    FileRow& setChecked(bool b)                          { checked_ = b; return *this; }
    FileRow& onToggle(std::function<void(bool)> cb)      { toggleCb_ = std::move(cb); return *this; }

    // V1.1.5 : kind + nombre de fichiers (pour l'affichage "dossier · N fichiers").
    FileRow& setKind(Kind k)                             { kind_ = k; return *this; }
    FileRow& setFileCount(int n)                         { fileCount_ = n; return *this; }

    void handleEvent(const sf::Event& e);
    void draw(sf::RenderTarget& target) const;

private:
    sf::FloatRect bounds_{};
    std::string   name_;
    std::uint64_t size_{0};
    bool          hoverX_{false};
    bool          checked_{true};
    Kind          kind_{Kind::File};
    int           fileCount_{1};
    std::function<void()> cb_;
    std::function<void(bool)> toggleCb_;

    sf::FloatRect closeBtnBounds() const;
    sf::FloatRect checkboxBounds() const;
};

} // namespace ltr::ui
