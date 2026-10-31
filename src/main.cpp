#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/TextArea.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;
namespace fs = std::filesystem;

namespace {
    constexpr ccColor4B PANEL_COLOR = { 15, 25, 37, 250 };
    constexpr ccColor3B ACCENT = { 111, 241, 174 };
    constexpr ccColor3B TEXT_COLOR = { 232, 241, 246 };
    constexpr ccColor3B MUTED_COLOR = { 142, 162, 179 };

    std::string trim(std::string value) {
        auto first = std::find_if_not(value.begin(), value.end(),
            [](unsigned char c) { return std::isspace(c); });
        auto last = std::find_if_not(value.rbegin(), value.rend(),
            [](unsigned char c) { return std::isspace(c); }).base();
        if (first >= last) return {};
        return std::string(first, last);
    }

    std::string safeScriptName(std::string value) {
        std::string result;
        for (unsigned char c : value) {
            if (std::isalnum(c) || c == '_' || c == '-') result.push_back(static_cast<char>(c));
        }
        if (result.empty()) result = "my_script";
        if (result.size() > 32) result.resize(32);
        return result;
    }

    fs::path scriptsDirectory() {
        auto path = Mod::get()->getSaveDir() / "scripts";
        std::error_code error;
        fs::create_directories(path, error);
        return path;
    }

    std::string joinLines(std::vector<std::string> const& lines) {
        std::ostringstream stream;
        for (auto const& line : lines) stream << line << "\n";
        return stream.str();
    }

    bool parseCall(std::string const& source, std::string& name, std::string& argument) {
        auto line = trim(source);
        auto open = line.find('(');
        auto close = line.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close < open) return false;

        name = trim(line.substr(0, open));
        auto raw = trim(line.substr(open + 1, close - open - 1));
        if (name.empty()) return false;
        if (raw.empty()) {
            argument.clear();
            return true;
        }
        if (raw.size() < 2 || (raw.front() != '"' && raw.front() != '\'') ||
            raw.back() != raw.front()) return false;

        char quote = raw.front();
        argument.clear();
        for (size_t i = 1; i + 1 < raw.size(); ++i) {
            char c = raw[i];
            if (c == '\\' && i + 2 < raw.size()) {
                char next = raw[++i];
                switch (next) {
                    case 'n': argument.push_back('\n'); break;
                    case 't': argument.push_back('\t'); break;
                    default: argument.push_back(next); break;
                }
            } else if (c == quote) {
                return false;
            } else {
                argument.push_back(c);
            }
        }
        return true;
    }

    void showGameToast(std::string const& message) {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (!scene) return;

        auto label = CCLabelBMFont::create(message.c_str(), "bigFont.fnt");
        if (!label) return;
        auto size = CCDirector::sharedDirector()->getWinSize();
        label->setPosition({ size.width / 2.f, size.height * .80f });
        label->setScale(.55f);
        label->setColor(ACCENT);
        label->setOpacity(0);
        scene->addChild(label, 10000);
        label->runAction(CCSequence::create(
            CCFadeIn::create(.18f),
            CCDelayTime::create(2.0f),
            CCFadeOut::create(.45f),
            CCRemoveSelf::create(),
            nullptr
        ));
    }
}

class GeodStudioLayer final : public CCLayer {
public:
    static GeodStudioLayer* create() {
        auto ret = new GeodStudioLayer();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

protected:
    bool init() {
        if (!CCLayer::init()) return false;

        auto win = CCDirector::sharedDirector()->getWinSize();
        m_panelWidth = std::min(650.f, win.width - 20.f);
        m_panelHeight = std::min(430.f, win.height - 18.f);
        m_left = (win.width - m_panelWidth) / 2.f;
        m_bottom = (win.height - m_panelHeight) / 2.f;
        m_centerX = win.width / 2.f;
        m_centerY = win.height / 2.f;

        auto shade = CCLayerColor::create(ccc4(0, 4, 9, 205), win.width, win.height);
        this->addChild(shade, -10);

        auto panel = CCLayerColor::create(PANEL_COLOR, m_panelWidth, m_panelHeight);
        panel->setPosition({ m_left, m_bottom });
        this->addChild(panel, -5);

        auto topLine = CCLayerColor::create(ccc4(111, 241, 174, 255), m_panelWidth - 28.f, 2.f);
        topLine->setPosition({ m_left + 14.f, m_bottom + m_panelHeight - 4.f });
        this->addChild(topLine, -4);

        auto title = CCLabelBMFont::create("GEOD  /  MOD STUDIO", "bigFont.fnt");
        title->setScale(.72f);
        title->setColor(ACCENT);
        title->setPosition({ m_centerX, m_bottom + m_panelHeight - 25.f });
        this->addChild(title);

        auto subtitle = CCLabelBMFont::create("MOBILE SCRIPT WORKSPACE", "chatFont.fnt");
        subtitle->setScale(.42f);
        subtitle->setColor(MUTED_COLOR);
        subtitle->setPosition({ m_centerX, m_bottom + m_panelHeight - 43.f });
        this->addChild(subtitle);

        m_menu = CCMenu::create();
        m_menu->setPosition({ 0.f, 0.f });
        this->addChild(m_menu, 20);

        m_content = CCLayer::create();
        this->addChild(m_content, 5);

        auto tabY = m_bottom + m_panelHeight - 76.f;
        addButton("EDITOR", menu_selector(GeodStudioLayer::onEditor), m_centerX - 105.f, tabY, .58f);
        addButton("CONSOLE", menu_selector(GeodStudioLayer::onConsole), m_centerX, tabY, .58f);
        addButton("SCRIPTS", menu_selector(GeodStudioLayer::onScripts), m_centerX + 105.f, tabY, .58f);
        addButton("CLOSE", menu_selector(GeodStudioLayer::onClose),
                  m_left + m_panelWidth - 37.f, m_bottom + m_panelHeight - 25.f, .48f);

        if (m_code.empty()) {
            m_code = {
                "print(\"Hello from Geod!\")",
                "toast(\"My first script is running\")",
                "notify(\"Script finished\")"
            };
        }
        renderEditor();
        return true;
    }

private:
    enum class Tab { Editor, Console, Scripts };

    Tab m_tab = Tab::Editor;
    CCMenu* m_menu = nullptr;
    CCLayer* m_content = nullptr;
    geode::TextInput* m_nameInput = nullptr;
    geode::TextInput* m_lineInput = nullptr;
    geode::SimpleTextArea* m_textArea = nullptr;
    std::vector<std::string> m_code;
    std::vector<std::string> m_console;
    std::vector<std::string> m_scripts;
    std::string m_scriptName = "hello";
    float m_panelWidth = 0.f;
    float m_panelHeight = 0.f;
    float m_left = 0.f;
    float m_bottom = 0.f;
    float m_centerX = 0.f;
    float m_centerY = 0.f;

    void addButton(char const* label, SEL_MenuHandler callback, float x, float y, float scale = .62f) {
        auto sprite = ButtonSprite::create(label, "goldFont.fnt", "GJ_button_01.png", .72f);
        if (!sprite) return;
        sprite->setScale(scale);
        auto item = CCMenuItemSpriteExtra::create(sprite, this, callback);
        item->setPosition({ x, y });
        m_menu->addChild(item);
    }

    void addLabel(std::string const& text, float x, float y, float scale = .48f,
                  ccColor3B color = TEXT_COLOR) {
        auto label = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
        if (!label) return;
        label->setScale(scale);
        label->setColor(color);
        label->setAnchorPoint({ 0.f, .5f });
        label->setPosition({ x, y });
        m_content->addChild(label);
    }

    void replaceContent() {
        m_content->removeAllChildrenWithCleanup(true);
        m_nameInput = nullptr;
        m_lineInput = nullptr;
        m_textArea = nullptr;
    }

    void addLog(std::string message) {
        m_console.push_back(std::move(message));
        if (m_console.size() > 24) m_console.erase(m_console.begin());
    }

    std::string consoleText() const {
        if (m_console.empty()) {
            return "GEOD CONSOLE\nReady. Tap RUN in the editor to execute a script.\nType help() in a script for available commands.";
        }
        std::ostringstream out;
        out << "GEOD CONSOLE\n";
        for (auto const& line : m_console) out << line << "\n";
        return out.str();
    }

    void renderEditor() {
        m_tab = Tab::Editor;
        replaceContent();

        addLabel("SCRIPT NAME", m_left + 18.f, m_bottom + 91.f, .42f, MUTED_COLOR);
        addLabel("ADD ONE COMMAND PER LINE", m_left + 18.f, m_bottom + 57.f, .42f, MUTED_COLOR);

        auto code = joinLines(m_code);
        if (code.empty()) code = "-- Add a command below";
        m_textArea = geode::SimpleTextArea::create(code, "chatFont.fnt", .48f, m_panelWidth - 54.f);
        m_textArea->setMaxLines(7);
        m_textArea->setColor(ccc4(222, 237, 244, 255));
        m_textArea->setPosition({ m_centerX, m_bottom + 156.f });
        m_content->addChild(m_textArea);

        m_nameInput = geode::TextInput::create(m_panelWidth * .27f, "script name", "chatFont.fnt");
        m_nameInput->setCommonFilter(geode::CommonFilter::ID);
        m_nameInput->setMaxCharCount(32);
        m_nameInput->setTextAlign(geode::TextInputAlign::Left);
        m_nameInput->setString(m_scriptName);
        m_nameInput->setPosition({ m_left + m_panelWidth * .17f, m_bottom + 75.f });
        m_nameInput->setCallback([this](std::string const& value) {
            m_scriptName = safeScriptName(value);
        });
        m_content->addChild(m_nameInput);

        m_lineInput = geode::TextInput::create(m_panelWidth * .48f, "print(\"Hello\")", "chatFont.fnt");
        m_lineInput->setCommonFilter(geode::CommonFilter::Any);
        m_lineInput->setMaxCharCount(180);
        m_lineInput->setTextAlign(geode::TextInputAlign::Left);
        m_lineInput->setPosition({ m_left + m_panelWidth * .55f, m_bottom + 75.f });
        m_content->addChild(m_lineInput);

        float y = m_bottom + 24.f;
        addButton("ADD", menu_selector(GeodStudioLayer::onAddLine), m_centerX - 132.f, y, .52f);
        addButton("SAVE", menu_selector(GeodStudioLayer::onSave), m_centerX - 44.f, y, .52f);
        addButton("LOAD", menu_selector(GeodStudioLayer::onLoad), m_centerX + 44.f, y, .52f);
        addButton("RUN", menu_selector(GeodStudioLayer::onRun), m_centerX + 132.f, y, .52f);
    }

    void renderConsole() {
        m_tab = Tab::Console;
        replaceContent();

        addLabel("OUTPUT / RUNTIME LOG", m_left + 20.f, m_bottom + m_panelHeight - 105.f,
                 .50f, ACCENT);
        m_textArea = geode::SimpleTextArea::create(consoleText(), "chatFont.fnt", .52f,
                                                    m_panelWidth - 48.f);
        m_textArea->setMaxLines(15);
        m_textArea->setColor(ccc4(211, 228, 237, 255));
        m_textArea->setPosition({ m_centerX, m_centerY + 2.f });
        m_content->addChild(m_textArea);

        addButton("CLEAR LOG", menu_selector(GeodStudioLayer::onClearConsole),
                  m_centerX, m_bottom + 24.f, .62f);
    }

    void renderScripts() {
        m_tab = Tab::Scripts;
        replaceContent();

        addLabel("SAVED .GDS SCRIPTS", m_left + 20.f, m_bottom + m_panelHeight - 105.f,
                 .50f, ACCENT);

        m_scripts.clear();
        std::error_code error;
        for (auto const& entry : fs::directory_iterator(scriptsDirectory(), error)) {
            if (entry.is_regular_file() && entry.path().extension() == ".gds") {
                m_scripts.push_back(entry.path().stem().string());
            }
        }
        std::sort(m_scripts.begin(), m_scripts.end());

        std::string list;
        if (m_scripts.empty()) {
            list = "No saved scripts yet.\nOpen EDITOR, enter a name and press SAVE.";
        } else {
            for (auto const& name : m_scripts) list += name + ".gds\n";
        }

        auto area = geode::SimpleTextArea::create(list, "chatFont.fnt", .55f,
                                                  m_panelWidth - 48.f);
        area->setMaxLines(12);
        area->setColor(ccc4(216, 232, 240, 255));
        area->setPosition({ m_centerX, m_centerY + 6.f });
        m_content->addChild(area);

        addButton("OPEN EDITOR", menu_selector(GeodStudioLayer::onEditor),
                  m_centerX - 72.f, m_bottom + 24.f, .55f);
        addButton("REFRESH", menu_selector(GeodStudioLayer::onScripts),
                  m_centerX + 72.f, m_bottom + 24.f, .55f);
    }

    void executeScript() {
        addLog("[run] " + safeScriptName(m_scriptName) + ".gds");
        size_t executed = 0;

        for (auto const& raw : m_code) {
            auto line = trim(raw);
            if (line.empty() || line.starts_with("--")) continue;

            std::string command;
            std::string argument;
            if (!parseCall(line, command, argument)) {
                addLog("[error] Invalid syntax: " + line);
                continue;
            }

            if (command == "print") {
                addLog("[script] " + argument);
                ++executed;
            } else if (command == "notify") {
                FLAlertLayer::create("Geod Script", argument, "OK")->show();
                addLog("[notify] " + argument);
                ++executed;
            } else if (command == "toast") {
                showGameToast(argument);
                addLog("[toast] " + argument);
                ++executed;
            } else if (command == "clear") {
                m_console.clear();
                addLog("[console] cleared by script");
                ++executed;
            } else if (command == "help") {
                addLog("API: print(\"text\"), notify(\"text\"), toast(\"text\"), clear(), help()");
                ++executed;
            } else {
                addLog("[error] Unknown API call: " + command);
            }
        }

        addLog("[done] " + std::to_string(executed) + " command(s) executed");
    }

    void onEditor(CCObject*) { renderEditor(); }
    void onConsole(CCObject*) { renderConsole(); }
    void onScripts(CCObject*) { renderScripts(); }

    void onClose(CCObject*) {
        this->removeFromParentAndCleanup(true);
    }

    void onAddLine(CCObject*) {
        if (!m_lineInput) return;
        auto line = trim(m_lineInput->getString());
        if (line.empty()) {
            addLog("[editor] enter a command first");
            return;
        }
        if (m_code.size() >= 80) {
            addLog("[editor] 80 line limit reached");
            return;
        }
        m_code.push_back(line);
        renderEditor();
    }

    void onSave(CCObject*) {
        if (m_nameInput) m_scriptName = safeScriptName(m_nameInput->getString());
        auto path = scriptsDirectory() / (m_scriptName + ".gds");
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            addLog("[error] could not write " + path.string());
            return;
        }
        file << joinLines(m_code);
        file.close();
        addLog("[saved] " + m_scriptName + ".gds");
        renderConsole();
    }

    void onLoad(CCObject*) {
        if (m_nameInput) m_scriptName = safeScriptName(m_nameInput->getString());
        auto path = scriptsDirectory() / (m_scriptName + ".gds");
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            addLog("[error] script not found: " + m_scriptName + ".gds");
            renderConsole();
            return;
        }

        m_code.clear();
        std::string line;
        while (std::getline(file, line) && m_code.size() < 80) {
            m_code.push_back(line);
        }
        addLog("[loaded] " + m_scriptName + ".gds");
        renderEditor();
    }

    void onRun(CCObject*) {
        if (m_nameInput) m_scriptName = safeScriptName(m_nameInput->getString());
        executeScript();
        renderConsole();
    }

    void onClearConsole(CCObject*) {
        m_console.clear();
        addLog("[console] ready");
        renderConsole();
    }
};

class $modify(GeodMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto menu = this->getChildByID("bottom-menu");
        if (menu) {
            auto sprite = ButtonSprite::create("GEOD", "goldFont.fnt", "GJ_button_04.png", .7f);
            auto button = CCMenuItemSpriteExtra::create(
                sprite, this, menu_selector(GeodMenuLayer::onOpenGeod)
            );
            button->setID("geod-studio-button"_spr);
            menu->addChild(button);
            menu->updateLayout();
        }
        return true;
    }

    void onOpenGeod(CCObject*) {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene) scene->addChild(GeodStudioLayer::create(), 10000);
    }
};
